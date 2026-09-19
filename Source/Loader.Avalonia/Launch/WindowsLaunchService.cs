#if WINDOWS
using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;

namespace Loader
{
  internal static class WindowsLaunchService
  {
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct STARTUPINFO
    {
      public uint cb;
      public string lpReserved;
      public string lpDesktop;
      public string lpTitle;
      public uint dwX;
      public uint dwY;
      public uint dwXSize;
      public uint dwYSize;
      public uint dwXCountChars;
      public uint dwYCountChars;
      public uint dwFillAttribute;
      public uint dwFlags;
      public short wShowWindow;
      public short cbReserved2;
      public IntPtr lpReserved2;
      public IntPtr hStdInput;
      public IntPtr hStdOutput;
      public IntPtr hStdError;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct PROCESS_INFORMATION
    {
      public IntPtr hProcess;
      public IntPtr hThread;
      public uint dwProcessId;
      public uint dwThreadId;
    }

    [Flags]
    private enum ProcessCreationFlags : uint
    {
      ZERO_FLAG = 0x00000000
    }

    [Flags]
    private enum AllocationType
    {
      Commit = 0x1000,
      Reserve = 0x2000
    }

    [Flags]
    private enum MemoryProtection
    {
      ReadWrite = 0x04
    }

    [DllImport("kernel32.dll")]
    private static extern bool CreateProcess(
      string lpApplicationName,
      string lpCommandLine,
      IntPtr lpProcessAttributes,
      IntPtr lpThreadAttributes,
      bool bInheritHandles,
      ProcessCreationFlags dwCreationFlags,
      IntPtr lpEnvironment,
      string lpCurrentDirectory,
      ref STARTUPINFO lpStartupInfo,
      out PROCESS_INFORMATION lpProcessInformation);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool WriteProcessMemory(
      IntPtr hProcess,
      IntPtr lpBaseAddress,
      byte[] lpBuffer,
      uint nSize,
      out int lpNumberOfBytesWritten);

    [DllImport("kernel32", CharSet = CharSet.Ansi, ExactSpelling = true, SetLastError = true)]
    private static extern IntPtr GetProcAddress(IntPtr hModule, string procName);

    [DllImport("kernel32.dll", EntryPoint = "GetModuleHandleA", SetLastError = true)]
    private static extern IntPtr GetModuleHandle(string moduleName);

    [DllImport("kernel32.dll", SetLastError = true, ExactSpelling = true)]
    private static extern IntPtr VirtualAllocEx(
      IntPtr hProcess,
      IntPtr lpAddress,
      uint dwSize,
      uint flAllocationType,
      uint flProtect);

    [DllImport("kernel32.dll")]
    private static extern IntPtr CreateRemoteThread(
      IntPtr hProcess,
      IntPtr lpThreadAttributes,
      uint dwStackSize,
      IntPtr lpStartAddress,
      IntPtr lpParameter,
      uint dwCreationFlags,
      IntPtr lpThreadId);

    public static bool TryLaunch(
      ServerConfig config,
      string exeLocation,
      string machinePublicIp,
      string machinePrivateIp,
      bool useSeparateSaveFiles,
      DarkSoulsLoadConfig loadConfig,
      out string? errorMessage)
    {
      if (string.IsNullOrEmpty(config.PublicKey))
      {
        errorMessage = "no_public_key_available";
        return false;
      }

      string connectionHostname = ResolveConnectIp(config, machinePublicIp, machinePrivateIp);

      string exeDirectory = Path.GetDirectoryName(exeLocation) ?? string.Empty;
      string appIdFile = Path.Join(exeDirectory, "steam_appid.txt");
      try
      {
        File.WriteAllText(appIdFile, loadConfig.SteamAppId.ToString());
      }
      catch (Exception ex)
      {
        errorMessage = $"failed_to_write_steam_appid: {ex.Message}";
        return false;
      }

      STARTUPINFO startupInfo = new STARTUPINFO();
      startupInfo.cb = (uint)Marshal.SizeOf<STARTUPINFO>();
      PROCESS_INFORMATION processInfo;

      bool result = CreateProcess(
        null,
        "\"" + exeLocation + "\"",
        IntPtr.Zero,
        IntPtr.Zero,
        false,
        ProcessCreationFlags.ZERO_FLAG,
        IntPtr.Zero,
        exeDirectory,
        ref startupInfo,
        out processInfo);

      if (!result)
      {
        errorMessage = "failed_to_run_game_exe";
        return false;
      }

      if (loadConfig.UseInjector)
      {
        if (!TryRunInjector(config, connectionHostname, useSeparateSaveFiles, processInfo, out errorMessage))
        {
          return false;
        }
      }
      else
      {
        if (!TryPatchServerInfo(config, connectionHostname, processInfo, loadConfig, out errorMessage))
        {
          return false;
        }
      }

      errorMessage = null;
      return true;
    }

    private static string ResolveConnectIp(ServerConfig config, string machinePublicIp, string machinePrivateIp)
    {
      string connectionHostname = config.Hostname;
      string hostnameIp = NetUtils.HostnameToIPv4(config.Hostname);
      string privateHostnameIp = NetUtils.HostnameToIPv4(config.PrivateHostname);

      if (hostnameIp == machinePublicIp)
      {
        connectionHostname = privateHostnameIp == machinePrivateIp ? "127.0.0.1" : config.PrivateHostname;
      }

      return connectionHostname;
    }

    private static bool TryRunInjector(
      ServerConfig config,
      string connectionHostname,
      bool useSeparateSaveFiles,
      PROCESS_INFORMATION processInfo,
      out string? errorMessage)
    {
      string? directoryPath = AppContext.BaseDirectory;
      string injectorPath = Path.Join(directoryPath, "Injector.dll");
      string injectorConfigPath = Path.Join(directoryPath, "Injector.config");

      while (!File.Exists(injectorPath))
      {
        directoryPath = Path.GetDirectoryName(directoryPath);
        if (string.IsNullOrEmpty(directoryPath))
        {
          errorMessage = "error_injector";
          return false;
        }

        injectorPath = Path.Join(directoryPath, "Injector.dll");
        injectorConfigPath = Path.Join(directoryPath, "Injector.config");
      }

      var injectConfig = new InjectionConfig
      {
        ServerName = config.Name,
        ServerPublicKey = config.PublicKey,
        ServerHostname = connectionHostname,
        ServerPort = config.Port,
        ServerGameType = config.GameType,
        EnableSeparateSaveFiles = useSeparateSaveFiles
      };

      try
      {
        File.WriteAllText(injectorConfigPath, injectConfig.ToJson());
      }
      catch (Exception ex)
      {
        errorMessage = $"failed_to_write_injector_config: {ex.Message}";
        return false;
      }

      IntPtr modulePtr = GetModuleHandle("kernel32.dll");
      if (modulePtr == IntPtr.Zero)
      {
        errorMessage = "kernel32_missing";
        return false;
      }

      IntPtr loadLibraryPtr = GetProcAddress(modulePtr, "LoadLibraryW");
      if (loadLibraryPtr == IntPtr.Zero)
      {
        errorMessage = "loadlibrary_missing";
        return false;
      }

      byte[] injectorPathBuffer = System.Text.Encoding.Unicode.GetBytes(injectorPath + "\0");
      IntPtr pathAddress = IntPtr.Zero;
      for (int i = 0; i < 32 && pathAddress == IntPtr.Zero; i++)
      {
        pathAddress = VirtualAllocEx(
          processInfo.hProcess,
          IntPtr.Zero,
          (uint)injectorPathBuffer.Length,
          (uint)(AllocationType.Reserve | AllocationType.Commit),
          (uint)MemoryProtection.ReadWrite);
        if (pathAddress == IntPtr.Zero)
        {
          Thread.Sleep(500);
        }
      }

      if (pathAddress == IntPtr.Zero)
      {
        errorMessage = "allocation_failed";
        return false;
      }

      if (!WriteProcessMemory(processInfo.hProcess, pathAddress, injectorPathBuffer, (uint)injectorPathBuffer.Length, out _))
      {
        errorMessage = "write_memory_failed";
        return false;
      }

      IntPtr threadHandle = CreateRemoteThread(
        processInfo.hProcess,
        IntPtr.Zero,
        0,
        loadLibraryPtr,
        pathAddress,
        0,
        IntPtr.Zero);
      if (threadHandle == IntPtr.Zero)
      {
        errorMessage = "remote_thread_failed";
        return false;
      }

      errorMessage = null;
      return true;
    }

    private static bool TryPatchServerInfo(
      ServerConfig config,
      string connectionHostname,
      PROCESS_INFORMATION processInfo,
      DarkSoulsLoadConfig loadConfig,
      out string? errorMessage)
    {
      byte[] dataBlock = PatchingUtils.MakeEncryptedServerInfo(connectionHostname, config.PublicKey, loadConfig.Key);
      if (dataBlock == null)
      {
        errorMessage = "Failed to encode server info patch. Potentially server information is too long to fit into the space available.";
        return false;
      }

      for (int i = 0; i < 32; i++)
      {
        IntPtr baseAddress = GetProcessModuleBaseAddress((int)processInfo.dwProcessId);
        IntPtr patchAddress = (IntPtr)loadConfig.ServerInfoAddress;
        if (loadConfig.UsesASLR)
        {
          patchAddress = (IntPtr)((ulong)baseAddress + (ulong)patchAddress);
        }

        if (WriteProcessMemory(processInfo.hProcess, patchAddress, dataBlock, (uint)dataBlock.Length, out int bytesWritten) && bytesWritten == dataBlock.Length)
        {
          errorMessage = null;
          return true;
        }

        if (i == 31)
        {
          errorMessage = "Failed to write full patch to memory. Game may or may not work.";
          return false;
        }

        Thread.Sleep(500);
      }

      errorMessage = "unknown_patch_failure";
      return false;
    }

    private static IntPtr GetProcessModuleBaseAddress(int processId)
    {
      try
      {
        using Process process = Process.GetProcessById(processId);
        if (process.MainModule != null)
        {
          return process.MainModule.BaseAddress;
        }

        if (process.Modules.Count > 0)
        {
          return process.Modules[0].BaseAddress;
        }
      }
      catch
      {
        // Process may have exited or is not accessible
      }

      return IntPtr.Zero;
    }
  }
}
#endif
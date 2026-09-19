namespace Loader
{
  /// <summary>Launches and configures a game through the Windows native APIs.</summary>
  internal interface IWindowsLaunchService
  {
    /// <summary>Starts the game and applies its server configuration.</summary>
    /// <param name="config">The selected server.</param>
    /// <param name="exeLocation">The validated game executable path.</param>
    /// <param name="machinePublicIp">The local machine's public IPv4 address.</param>
    /// <param name="machinePrivateIp">The local machine's private IPv4 address.</param>
    /// <param name="useSeparateSaveFiles">Whether to isolate saves for this server.</param>
    /// <param name="loadConfig">The executable configuration resolved during preparation.</param>
    /// <param name="errorMessage">The failure reason, or null on success.</param>
    /// <returns>True if the game was launched and configured successfully.</returns>
    bool TryLaunch(
      ServerConfig config,
      string exeLocation,
      string machinePublicIp,
      string machinePrivateIp,
      bool useSeparateSaveFiles,
      DarkSoulsLoadConfig loadConfig,
      out string? errorMessage);
  }
}

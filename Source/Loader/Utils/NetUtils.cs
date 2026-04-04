using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;

namespace Loader
{
  public static class NetUtils
  {
    private const string PublicIpServiceUrl = "https://api.ipify.org";

    public static string HostnameToIPv4(string Hostname)
    {
      try
      {
        IPAddress? Address = Dns.GetHostAddresses(Hostname).FirstOrDefault(Addr => Addr.AddressFamily == AddressFamily.InterNetwork);
        return Address?.ToString() ?? string.Empty;
      }
      catch (SocketException)
      {
        // DNS lookup failed.
      }
      catch (ArgumentException)
      {
        // Placeholder catch all exception ...
      }
      return "";
    }

    public static string GetMachineIPv4(bool GetPublicAddress)
    {
      try
      {
        if (GetPublicAddress)
        {
          using (HttpClient client = new HttpClient())
          {
            // synchronous call for simplicity
            return client.GetStringAsync(PublicIpServiceUrl).GetAwaiter().GetResult();
          }
        }
        else
        {
          return HostnameToIPv4(Dns.GetHostName());
        }
      }
      catch (HttpRequestException)
      {
        // Failed to retrieve public IP.
      }
      catch (TaskCanceledException)
      {
        // Request timed out or was cancelled.
      }
      catch (SocketException)
      {
        // Network lookup failed.
      }
      return "";
    }
  }
}

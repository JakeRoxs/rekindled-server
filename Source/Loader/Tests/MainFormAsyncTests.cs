using System.Threading;
using System.Threading.Tasks;

using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Loader.Tests
{
  [TestClass]
  public class MainFormAsyncTests
  {
    private class TestMainForm : Loader.MainForm
    {
      private TestMainForm() : base(_ => "127.0.0.1")
      {
      }

      public static TestMainForm Create()
      {
        var context = SynchronizationContext.Current;
        try
        {
          return new TestMainForm();
        }
        finally
        {
          // These tests have no UI message loop to service WinForms continuations.
          SynchronizationContext.SetSynchronizationContext(context);
        }
      }

      public Task<string> TestResolveConnectIpAsync(ServerConfig config, CancellationToken cancellationToken)
      {
        return ResolveConnectIpAsync(config, cancellationToken);
      }

      public Task<string> TestGetPublicKeyAsync(string id, string password, CancellationToken cancellationToken)
      {
        return GetPublicKeyAsync(id, password, cancellationToken);
      }

      public Task SystemQueryServersAsync(CancellationToken cancellationToken)
      {
        return QueryServersAsync(cancellationToken);
      }
    }

    [TestMethod]
    public async Task ResolveConnectIpAsync_CancelsImmediately()
    {
      using var form = TestMainForm.Create();
      using var cts = new CancellationTokenSource();
      cts.Cancel();

      await Assert.ThrowsExceptionAsync<TaskCanceledException>(
          () => form.TestResolveConnectIpAsync(new ServerConfig(), cts.Token));
    }

    [TestMethod]
    public async Task GetPublicKeyAsync_CancelsImmediately()
    {
      using var form = TestMainForm.Create();
      using var cts = new CancellationTokenSource();
      cts.Cancel();

      await Assert.ThrowsExceptionAsync<TaskCanceledException>(
          () => form.TestGetPublicKeyAsync("id", "", cts.Token));
    }
  }
}

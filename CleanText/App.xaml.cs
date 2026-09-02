using System.Windows;

namespace CleanText;

public partial class App : Application
{
    protected override void OnStartup(StartupEventArgs e)
    {
        DispatcherUnhandledException += (_, args) =>
        {
            try { System.IO.File.WriteAllText("F:\\StarAway\\cleantext_error.log",
                args.Exception.ToString()); } catch { }
            args.Handled = false;
        };
        base.OnStartup(e);
    }
}

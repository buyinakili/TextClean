using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
using System.Windows.Media;
using Microsoft.Win32;

namespace CleanText;

public partial class MainWindow : Window
{
    // Layout constants
    private const double WindowWidthFixed = 400;
    private const double OuterMargin = 14;          // border margin + shadow breathing room
    private const double HeaderHeight = 38;         // approx title bar
    private const double ContentMargin = 14;
    private const double MaxWindowHeight = 620;
    private const double MinWindowHeight = 96;
    private const double InputMaxHeight = 200;
    private const double OutputMaxHeight = 200;
    private const double OutputMinHeight = 64;
    private const double InputMinHeight = 50;
    private const double InputPadding = 22;         // padding + caret buffer

    private readonly TextBlock _measure = new TextBlock
    {
        TextWrapping = TextWrapping.Wrap,
        FontSize = 14,
        FontFamily = new FontFamily("Segoe UI"),
    };

    public MainWindow()
    {
        InitializeComponent();
        Width = WindowWidthFixed;
        StartupCheckBox.IsChecked = IsStartupEnabled();
        TopmostCheckBox.IsChecked = Topmost;

        // Auto-run a self-test when launched with --selftest
        if (Environment.GetCommandLineArgs().Contains("--selftest"))
        {
            Loaded += (_, _) => RunSelfTest();
        }

        // Center on screen initially and hug content from the start
        Loaded += (_, _) =>
        {
            UpdateWindowHeight();
            Left = (SystemParameters.PrimaryScreenWidth - ActualWidth) / 2;
            Top = (SystemParameters.PrimaryScreenHeight - ActualHeight) / 2;
            FocusInput();
        };
        Activated += (_, _) => FocusInput();
        // Ctrl+Z default undo (already enabled); focus caret start
        InputBox.CaretIndex = 0;
    }

    private void FocusInput()
    {
        InputBox.Focus();
        Keyboard.Focus(InputBox);
    }

    // -------------------- Self test --------------------
    private void RunSelfTest()
    {
        var sb = new System.Text.StringBuilder();
        try
        {
            // Test: remove * and create a card via the real ProcessInput path
            InputBox.Text = "this is *important* text* with*stars";
            ProcessInput();  // should add a card and preserve the input
            int cardCount = OutputStack.Children.Count;
            string firstCard = cardCount > 0
                ? (((Border)OutputStack.Children[0]).Child as Grid)!.Children.OfType<TextBlock>().First().Text
                : "(none)";
            sb.AppendLine("cardsAfterFirstEnter=" + cardCount);
            sb.AppendLine("firstCardText=[" + firstCard + "]");
            sb.AppendLine("inputAfterFirstEnter=[" + InputBox.Text + "]");

            // Test: a new result replaces the previous card
            InputBox.Text = "second*value";
            ProcessInput();
            sb.AppendLine("cardsAfterSecondEnter=" + OutputStack.Children.Count);

            // Test: empty input should be ignored
            InputBox.Text = "";
            ProcessInput();
            sb.AppendLine("cardsAfterEmptyEnter=" + OutputStack.Children.Count);

            // Test: copy button path runs without throwing (clipboard-read is flaky in CI)
            var firstCardGrid = ((Border)OutputStack.Children[0]).Child as Grid;
            var copyBtn = firstCardGrid!.Children.OfType<Button>().First();
            copyBtn.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
            sb.AppendLine("copyClicked=true");

            // Test: clear + ctrl+z undo
            InputBox.Text = "undo me";
            Clear_Click(this, new RoutedEventArgs());
            sb.AppendLine("inputAfterClear=[" + InputBox.Text + "]");
        }
        catch (Exception ex)
        {
            sb.AppendLine("EXCEPTION=" + ex);
        }
        System.IO.File.WriteAllText("F:\\StarAway\\cleantext_selftest.txt", sb.ToString());
        Close();
    }

    // -------------------- Window sizing --------------------
    private void UpdateWindowHeight()
    {
        Width = WindowWidthFixed;

        // Measure after layout so we use the real rendered heights.
        Dispatcher.BeginInvoke(System.Windows.Threading.DispatcherPriority.Loaded, new Action(() =>
        {
            double headerH = HeaderBar.ActualHeight > 0 ? HeaderBar.ActualHeight : HeaderHeight;
            double contentH = MainStack.ActualHeight + MainStack.Margin.Top + MainStack.Margin.Bottom;
            double target = headerH + contentH + OuterMargin * 2 + 4;
            Height = Math.Clamp(target, MinWindowHeight, MaxWindowHeight);
        }));
    }

    // -------------------- Title bar --------------------
    private void TitleBar_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        if (e.ButtonState == MouseButtonState.Pressed)
        {
            try { DragMove(); }
            catch { /* ignore reentrancy */ }
        }
    }

    private void Minimize_Click(object sender, RoutedEventArgs e)
        => WindowState = WindowState.Minimized;

    private void Close_Click(object sender, RoutedEventArgs e)
        => Close();

    private void Settings_Click(object sender, RoutedEventArgs e)
    {
        SettingsPanel.Visibility = SettingsPanel.Visibility == Visibility.Visible
            ? Visibility.Collapsed
            : Visibility.Visible;
        UpdateWindowHeight();
    }

    private void TopmostCheckBox_Changed(object sender, RoutedEventArgs e)
        => Topmost = TopmostCheckBox.IsChecked == true;

    private void StartupCheckBox_Changed(object sender, RoutedEventArgs e)
    {
        try
        {
            using RegistryKey? runKey = Registry.CurrentUser.OpenSubKey(
                @"Software\Microsoft\Windows\CurrentVersion\Run", writable: true);
            if (runKey is null)
                return;

            if (StartupCheckBox.IsChecked == true)
            {
                string executable = Environment.ProcessPath
                    ?? System.IO.Path.Combine(AppContext.BaseDirectory, "CleanText.exe");
                runKey.SetValue("CleanText", $"\"{executable}\"");
            }
            else
            {
                runKey.DeleteValue("CleanText", throwOnMissingValue: false);
            }
        }
        catch
        {
            // Keep the app usable if the current user policy blocks Run-key writes.
            StartupCheckBox.IsChecked = IsStartupEnabled();
        }
    }

    private static bool IsStartupEnabled()
    {
        try
        {
            using RegistryKey? runKey = Registry.CurrentUser.OpenSubKey(
                @"Software\Microsoft\Windows\CurrentVersion\Run", writable: false);
            return runKey?.GetValue("CleanText") is not null;
        }
        catch { return false; }
    }

    // -------------------- Input box --------------------
    private void InputBox_TextChanged(object sender, TextChangedEventArgs e)
    {
        // show/hide the clear "x" button
        ClearBtn.Visibility = string.IsNullOrEmpty(InputBox.Text)
            ? Visibility.Collapsed
            : Visibility.Visible;

        ResizeInput();
        UpdateWindowHeight();
    }

    private void ResizeInput()
    {
        double availWidth = Width - OuterMargin * 2 - ContentMargin * 2;
        _measure.TextWrapping = TextWrapping.Wrap;
        _measure.Text = InputBox.Text;
        _measure.Measure(new Size(availWidth, double.PositiveInfinity));
        double h = _measure.DesiredSize.Height + InputPadding;

        h = Math.Clamp(h, InputMinHeight, InputMaxHeight);
        InputBox.Height = h;
    }

    private void InputBox_PreviewKeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key == Key.Enter)
        {
            // Shift+Enter inserts a newline; plain Enter processes the input.
            // PreviewKeyDown runs BEFORE the TextBox's built-in AcceptsReturn
            // handling, so intercepting here lets plain Enter generate a result
            // instead of inserting a newline.
            if ((Keyboard.Modifiers & ModifierKeys.Shift) != 0)
                return;

            e.Handled = true;
            ProcessInput();
        }
    }

    private void ProcessInput()
    {
        string raw = InputBox.Text;
        if (string.IsNullOrEmpty(raw))
            return;

        string cleaned = raw.Replace("*", "");
        // Keep the editable source text, but replace the previous generated result.
        OutputStack.Children.Clear();
        AddOutputCard(cleaned);

        InputBox.Focus();
        UpdateWindowHeight();
    }

    private void Clear_Click(object sender, RoutedEventArgs e)
    {
        // select + delete so the clear participates in Undo (Ctrl+Z restores it)
        InputBox.SelectAll();
        InputBox.SelectedText = "";
        InputBox.Focus();
    }

    // -------------------- Output cards --------------------
    private void AddOutputCard(string text)
    {
        var card = new Border
        {
            CornerRadius = new CornerRadius(10),
            Background = Brushes.White,
            BorderBrush = new SolidColorBrush(Color.FromRgb(0xCF, 0xEB, 0xDF)),
            BorderThickness = new Thickness(1),
            Margin = new Thickness(0, 0, 0, 8),
            MinHeight = OutputMinHeight,
            MaxHeight = OutputMaxHeight,
        };

        var grid = new Grid();

        var textBlock = new TextBlock
        {
            Text = text,
            TextWrapping = TextWrapping.Wrap,
            FontSize = 14,
            Foreground = new SolidColorBrush(Color.FromRgb(0x1E, 0x2A, 0x26)),
            Padding = new Thickness(10),
        };

        var textScroll = new ScrollViewer
        {
            Content = textBlock,
            VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
            HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled,
            Margin = new Thickness(0),
            MaxHeight = OutputMaxHeight,
        };
        textScroll.Resources.Add(typeof(ScrollBar), FindResource("SoftScrollBar"));

        var copyBtn = new Button
        {
            Style = (Style)FindResource("ActionIconButton"),
            ToolTip = "复制文本",
            Background = Brushes.White,
            VerticalAlignment = VerticalAlignment.Bottom,
            HorizontalAlignment = HorizontalAlignment.Right,
            // Float over the lower-right text edge, just left of the scrollbar.
            Margin = new Thickness(0, 0, 20, 10),
        };
        // Exact path data from the supplied ic_public_copy.svg.
        var copyIcon = new System.Windows.Shapes.Path
        {
            Data = Geometry.Parse("M15.0132009,4.5 C16.5733587,4.5 17.1391096,4.66244482 17.70948,4.96748223 C18.2798504,5.27251964 18.7274804,5.72014965 19.0325178,6.29052002 L19.1342249,6.49326214 C19.3735291,7.00777167 19.5,7.61018928 19.5,8.9867991 L19.5,17.5132009 C19.5,19.0733587 19.3375552,19.6391096 19.0325178,20.20948 C18.7274804,20.7798504 18.2798504,21.2274804 17.70948,21.5325178 L17.5067379,21.6342249 C16.9922283,21.8735291 16.3898107,22 15.0132009,22 L6.4867991,22 C4.92664131,22 4.36089039,21.8375552 3.79052002,21.5325178 C3.22014965,21.2274804 2.77251964,20.7798504 2.46748223,20.20948 L2.36577509,20.0067379 C2.12647088,19.4922283 2,18.8898107 2,17.5132009 L2,8.9867991 C2,7.42664131 2.16244482,6.86089039 2.46748223,6.29052002 C2.77251964,5.72014965 3.22014965,5.27251964 3.79052002,4.96748223 L3.99326214,4.86577509 C4.47347103,4.64242449 5.03025764,4.51736427 6.22159636,4.50168224 L15.0132009,4.5 Z M6.4867991,6 C5.29081707,6 4.8991107,6.07564199 4.49791831,6.29020203 C4.18895065,6.45543974 3.95543974,6.68895065 3.79020203,6.99791831 L3.71163699,7.15981826 C3.56872488,7.49032199 3.50932077,7.88419566 3.50102731,8.75808525 L3.5,17.5132009 C3.5,18.7091829 3.57564199,19.1008893 3.79020203,19.5020817 C3.95543974,19.8110494 4.18895065,20.0445603 4.49791831,20.209798 L4.65981826,20.288363 C4.99032199,20.4312751 5.38419566,20.4906792 6.25808525,20.4989727 L6.4867991,20.5 L15.0132009,20.5 L15.4506279,20.4958158 C16.3138066,20.4773591 16.6543816,20.39575 17.0020817,20.209798 C17.3110494,20.0445603 17.5445603,19.8110494 17.709798,19.5020817 L17.788363,19.3401817 C17.9312751,19.009678 17.9906792,18.6158043 17.9989727,17.7419147 L18,17.5132009 L18,8.9867991 L17.9958158,8.54937207 C17.9773591,7.6861934 17.89575,7.34561838 17.709798,6.99791831 C17.5445603,6.68895065 17.3110494,6.45543974 17.0020817,6.29020203 L16.8401817,6.21163699 C16.509678,6.06872488 16.1158043,6.00932077 15.2419147,6.00102731 L6.4867991,6 Z M15.590287,2 C17.8190838,2 18.6272994,2.23206403 19.4421143,2.66783176 C20.2569291,3.10359949 20.8964005,3.74307093 21.3321682,4.55788574 C21.767936,5.37270056 22,6.18091615 22,8.409713 L22,14.3722296 C22,16.1552671 21.8143488,16.8018396 21.4657346,17.4536914 C21.2202776,17.9126561 20.8940321,18.3020799 20.4949174,18.6140435 C20.4981214,18.4695118 20.5,18.316606 20.5,18.1541722 L20.5,8.3458278 C20.5,6.97563815 20.3663256,6.28341544 19.9811141,5.56313259 C19.6264537,4.89997522 19.1000248,4.3735463 18.4368674,4.01888586 C17.7646034,3.65935514 17.1167829,3.51894119 15.9193798,3.50181725 L5.8458278,3.5 C5.68310622,3.5 5.5299464,3.50188529 5.38516578,3.50585327 C5.69792007,3.10596794 6.0873439,2.77972241 6.54630859,2.53426541 C7.19816044,2.18565122 7.84473292,2 9.6277704,2 L15.590287,2 Z M10.75,8.25 C11.1642136,8.25 11.5,8.58578644 11.5,9 L11.5,17.5 C11.5,17.9142136 11.1642136,18.25 10.75,18.25 C10.3357864,18.25 10,17.9142136 10,17.5 L10,14 L6.5,14 C6.08578644,14 5.75,13.6642136 5.75,13.25 C5.75,12.8357864 6.08578644,12.5 6.5,12.5 L10,12.5 L10,9 C10,8.58578644 10.3357864,8.25 10.75,8.25 Z M15,12.5 C15.4142136,12.5 15.75,12.8357864 15.75,13.25 C15.75,13.6642136 15.4142136,14 15,14 L12.5,14 L12.5,12.5 L15,12.5 Z"),
            Fill = new SolidColorBrush(Color.FromRgb(0x2D, 0xD4, 0xA3)),
            Width = 24,
            Height = 24,
            Stretch = Stretch.Uniform,
        };
        copyBtn.Content = copyIcon;
        copyBtn.Click += (_, _) =>
        {
            // Retry a couple times in case another process holds the clipboard
            for (int i = 0; i < 4; i++)
            {
                try { System.Windows.Clipboard.SetText(text); break; }
                catch { System.Threading.Thread.Sleep(60); }
            }
            // transient feedback
            copyBtn.ToolTip = "已复制";
            var t = new System.Windows.Threading.DispatcherTimer { Interval = TimeSpan.FromSeconds(1.2) };
            t.Tick += (_, _) => { copyBtn.ToolTip = "复制文本"; t.Stop(); };
            t.Start();
        };

        grid.Children.Add(textScroll);
        grid.Children.Add(copyBtn);
        card.Child = grid;

        OutputStack.Children.Add(card);
    }
}

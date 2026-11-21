using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace LfsPadSetup
{
    internal static class Ui
    {
        internal static bool English = System.Globalization.CultureInfo.CurrentUICulture.TwoLetterISOLanguageName != "tr";
        internal static string T(string tr, string en) { return English ? en : tr; }
    }

    internal static class Program
    {
        [STAThread]
        private static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            bool firstInstance;
            using (var instance = new Mutex(true, @"Local\LfsPadSetup", out firstInstance))
            {
                if (!firstInstance)
                {
                    MessageBox.Show(Ui.T("Bir LfsPad kurulum penceresi zaten açık.", "LfsPad Setup is already open."), "LfsPad");
                    return;
                }
                try
                {
                    Application.Run(new SetupWindow());
                }
                finally
                {
                    instance.ReleaseMutex();
                }
            }
        }
    }

    internal sealed class SetupWindow : Form
    {
        private readonly TextBox folder = new TextBox();
        private readonly Button browse = new Button { Text = Ui.T("Göz at", "Browse"), AutoSize = true };
        private readonly Button install = new Button { Text = Ui.T("Kur", "Install"), AutoSize = true };
        private readonly Button remove = new Button { Text = Ui.T("Kaldır", "Uninstall"), AutoSize = true };
        private readonly Label status = new Label { AutoSize = true, MaximumSize = new Size(505, 0) };
        private readonly ComboBox language = new ComboBox();
        private bool busy;

        internal SetupWindow()
        {
            Text = Ui.T("LfsPad Kurulumu", "LfsPad Setup");
            Font = SystemFonts.MessageBoxFont;
            AutoScaleMode = AutoScaleMode.Font;
            AutoSize = true;
            AutoSizeMode = AutoSizeMode.GrowAndShrink;
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;

            var layout = new TableLayoutPanel
            {
                AutoSize = true,
                AutoSizeMode = AutoSizeMode.GrowAndShrink,
                ColumnCount = 2,
                Dock = DockStyle.Fill,
                Padding = new Padding(18),
            };
            layout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 410));
            layout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 94));
            Controls.Add(layout);

            var title = new Label
            {
                Text = "LfsPad " + Application.ProductVersion,
                Font = new Font(Font.FontFamily, 18, FontStyle.Bold),
                AutoSize = true,
                Margin = new Padding(3, 0, 3, 12),
            };
            layout.Controls.Add(title, 0, 0);
            language.DropDownStyle = ComboBoxStyle.DropDownList;
            language.Dock = DockStyle.Fill;
            language.Items.AddRange(new object[] { "Türkçe", "English" });
            language.SelectedIndex = Ui.English ? 1 : 0;
            language.AccessibleName = "Dil / Language";
            layout.Controls.Add(language, 1, 0);
            var folderLabel = new Label { Text = Ui.T("Live for Speed 0.8C38 klasörü", "Live for Speed 0.8C38 folder"), AutoSize = true };
            layout.Controls.Add(folderLabel, 0, 1);
            layout.SetColumnSpan(folderLabel, 2);
            folder.AccessibleName = folderLabel.Text;
            folder.Dock = DockStyle.Fill;
            layout.Controls.Add(folder, 0, 2);
            browse.Dock = DockStyle.Fill;
            layout.Controls.Add(browse, 1, 2);

            status.Margin = new Padding(3, 10, 3, 9);
            status.MinimumSize = new Size(0, 32);
            layout.Controls.Add(status, 0, 3);
            layout.SetColumnSpan(status, 2);

            var footer = new TableLayoutPanel { AutoSize = true, ColumnCount = 2, Dock = DockStyle.Fill };
            footer.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            footer.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            layout.Controls.Add(footer, 0, 4);
            layout.SetColumnSpan(footer, 2);
            var license = new LinkLabel
            {
                Text = Ui.T("Üçüncü taraf lisansları", "Third-party licenses"),
                AutoSize = true,
                Anchor = AnchorStyles.Left,
            };
            footer.Controls.Add(license, 0, 0);
            var buttons = new FlowLayoutPanel
            {
                FlowDirection = FlowDirection.RightToLeft,
                AutoSize = true,
                AutoSizeMode = AutoSizeMode.GrowAndShrink,
                Anchor = AnchorStyles.Right,
                WrapContents = false,
                Margin = Padding.Empty,
            };
            footer.Controls.Add(buttons, 1, 0);
            buttons.Controls.Add(install);
            buttons.Controls.Add(remove);
            AcceptButton = install;

            language.SelectedIndexChanged += delegate {
                Ui.English = language.SelectedIndex == 1;
                Text = Ui.T("LfsPad Kurulumu", "LfsPad Setup");
                folder.AccessibleName = folderLabel.Text = Ui.T("Live for Speed 0.8C38 klasörü", "Live for Speed 0.8C38 folder");
                browse.Text = Ui.T("Göz at", "Browse");
                remove.Text = Ui.T("Kaldır", "Uninstall");
                license.Text = Ui.T("Üçüncü taraf lisansları", "Third-party licenses");
                RefreshActions();
            };
            browse.Click += delegate { ChooseFolder(); };
            install.Click += async delegate { await Apply(false); };
            remove.Click += async delegate { await Apply(true); };
            folder.TextChanged += delegate { RefreshActions(); };
            license.LinkClicked += delegate { ShowLicenses(); };
            FormClosing += delegate(object sender, FormClosingEventArgs e)
            {
                if (busy && e.CloseReason == CloseReason.UserClosing)
                    e.Cancel = true;
            };

            string defaultFolder = Path.Combine(Path.GetPathRoot(Environment.SystemDirectory), "LFS");
            folder.Text = HasGame(Environment.CurrentDirectory) ? Environment.CurrentDirectory : defaultFolder;
            RefreshActions();
        }

        private static bool HasGame(string root)
        {
            if (String.IsNullOrWhiteSpace(root))
                return false;
            try { return File.Exists(Path.Combine(root, "LFS.exe")); }
            catch (ArgumentException) { return false; }
        }

        private static bool HasInstallation(string root)
        {
            return File.Exists(Path.Combine(root, @"addons\LfsPad\installation.json"));
        }

        private void ChooseFolder()
        {
            using (var dialog = new FolderBrowserDialog())
            {
                dialog.Description = Ui.T("LFS.exe dosyasının bulunduğu klasörü seç.", "Choose the folder containing LFS.exe.");
                dialog.ShowNewFolderButton = false;
                if (Directory.Exists(folder.Text))
                    dialog.SelectedPath = folder.Text;
                if (dialog.ShowDialog(this) == DialogResult.OK)
                    folder.Text = dialog.SelectedPath;
            }
        }

        private void RefreshActions()
        {
            string root = folder.Text.Trim();
            bool valid = HasGame(root);
            bool installed = valid && HasInstallation(root);
            language.Enabled = folder.Enabled = browse.Enabled = !busy;
            install.Enabled = valid && !busy;
            remove.Enabled = installed && !busy;
            install.Text = installed ? Ui.T("Güncelle", "Update") : Ui.T("Kur", "Install");
            status.Text = valid ? Ui.T("Kurulumdan önce LFS’yi kapat.", "Close LFS before installing.") : Ui.T("LFS.exe’nin bulunduğu klasörü seç.", "Choose the folder containing LFS.exe.");
        }

        private async Task Apply(bool uninstall)
        {
            if (busy)
                return;
            try
            {
                string root = Path.GetFullPath(folder.Text.Trim());
                if (!HasGame(root))
                    throw new InvalidOperationException(Ui.T("Seçilen klasörde LFS.exe bulunamadı.", "LFS.exe was not found in the selected folder."));
                if (uninstall && MessageBox.Show(this, Ui.T("Bu klasördeki LfsPad kaldırılsın mı?", "Uninstall LfsPad from this folder?"),
                    "LfsPad", MessageBoxButtons.YesNo, MessageBoxIcon.Question,
                    MessageBoxDefaultButton.Button2) != DialogResult.Yes)
                    return;
                busy = true;
                RefreshActions();
                status.Text = uninstall ? Ui.T("LfsPad kaldırılıyor…", "Uninstalling LfsPad…") : HasInstallation(root) ? Ui.T("LfsPad güncelleniyor…", "Updating LfsPad…") : Ui.T("LfsPad kuruluyor…", "Installing LfsPad…");
                UseWaitCursor = true;
                string result = await Task.Run(() => RunOperation(root, uninstall));
                busy = false;
                RefreshActions();
                status.Text = result;
            }
            catch (Exception exception)
            {
                busy = false;
                RefreshActions();
                status.Text = Ui.T("İşlem tamamlanamadı.", "The operation could not be completed.");
                MessageBox.Show(this, exception.Message, Text, MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
            finally
            {
                UseWaitCursor = false;
            }
        }

        private static Stream GetResource(string name)
        {
            var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream(name);
            if (stream == null)
                throw new InvalidOperationException(Ui.T("Kurulum dosyası eksik. EXE’yi yeniden indir.", "The setup file is incomplete. Download it again."));
            return stream;
        }

        private void ShowLicenses()
        {
            var text = new System.Text.StringBuilder();
            foreach (var file in new[] { "Dear-ImGui-LICENSE.txt", "SDL-LICENSE.txt", "SDL-HIDAPI-LICENSE.txt" })
            {
                using (var reader = new StreamReader(GetResource(file)))
                    text.AppendLine(file).AppendLine().AppendLine(reader.ReadToEnd()).AppendLine();
            }
            using (var dialog = new Form { Text = Ui.T("Üçüncü taraf lisansları", "Third-party licenses"),
                                           StartPosition = FormStartPosition.CenterParent,
                                           ClientSize = new Size(590, 450) })
            {
                dialog.Controls.Add(new TextBox
                {
                    Text = text.ToString(),
                    ReadOnly = true,
                    Multiline = true,
                    ScrollBars = ScrollBars.Vertical,
                    Dock = DockStyle.Fill
                });
                dialog.ShowDialog(this);
            }
        }

        private static void Extract(string name, string destination)
        {
            using (Stream source = GetResource(name))
            using (FileStream target = File.Open(destination, FileMode.CreateNew, FileAccess.Write))
                source.CopyTo(target);
        }

        private static string QuotePath(string path)
        {
            // Preserve the trailing slash when quoting a drive or share root.
            return "\"" + path + new string('\\', path.Length - path.TrimEnd('\\').Length) + "\"";
        }

        private static string RunOperation(string root, bool uninstall)
        {
            bool update = !uninstall && HasInstallation(root);
            if (root.IndexOfAny(Path.GetInvalidPathChars()) >= 0 || root.Contains("\""))
                throw new InvalidOperationException(Ui.T("Oyun klasörünün yolu geçersiz.", "The game folder path is invalid."));
            string staging = Path.Combine(Path.GetTempPath(), "LfsPadSetup-" + Guid.NewGuid().ToString("N"));
            string tools = Path.Combine(staging, "tools");
            string scriptName = uninstall ? "uninstall.ps1" : "install.ps1";
            string script = Path.Combine(tools, scriptName);
            string common = Path.Combine(tools, "install-common.ps1");
            string dll = Path.Combine(staging, "dinput8.dll");
            string ini = Path.Combine(staging, "LfsPad.ini");
            try
            {
                Directory.CreateDirectory(tools);
                Extract("install-common.ps1", common);
                Extract(scriptName, script);
                if (!uninstall)
                {
                    Extract("dinput8.dll", dll);
                    Extract("LfsPad.ini", ini);
                    if (Ui.English)
                        File.WriteAllText(ini, File.ReadAllText(ini).Replace("Language=tr", "Language=en"));
                }
                string powershell = Path.Combine(Environment.SystemDirectory,
                    @"WindowsPowerShell\v1.0\powershell.exe");
                var start = new ProcessStartInfo
                {
                    FileName = powershell,
                    Arguments = "-NoLogo -NoProfile -NonInteractive -ExecutionPolicy RemoteSigned -File " +
                        QuotePath(script) + " -GameDirectory " + QuotePath(root) +
                        (update ? " -Update" : ""),
                    CreateNoWindow = true,
                    UseShellExecute = false,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    WorkingDirectory = staging,
                };
                // PowerShell 7 can supply a module path that omits Windows PowerShell's modules.
                start.EnvironmentVariables.Remove("PSModulePath");
                using (var process = Process.Start(start))
                {
                    Task<string> output = process.StandardOutput.ReadToEndAsync();
                    Task<string> errors = process.StandardError.ReadToEndAsync();
                    process.WaitForExit();
                    if (process.ExitCode != 0)
                    {
                        string detail = errors.Result.Trim();
                        if (detail.Contains("Close LFS before"))
                            detail = Ui.T("LFS açık. Oyunu kapatıp tekrar dene.", "Close LFS and try again.");
                        else if (detail.Contains("Only the verified 0.8C38"))
                            detail = Ui.T("Bu klasörde farklı bir LFS sürümü var. LFS 0.8C38’in kurulu olduğu klasörü seç.", "Select the folder containing LFS 0.8C38.");
                        if (detail.Length == 0)
                            detail = Ui.T("Kurulum betiği hata koduyla kapandı (", "Setup returned error code (") + process.ExitCode + ").";
                        throw new InvalidOperationException(detail);
                    }
                    if (uninstall)
                        return Ui.T("LfsPad kaldırıldı.", "LfsPad has been uninstalled.");
                    if (output.Result.Contains("already installed"))
                        return Ui.T("Bu LfsPad sürümü zaten kurulu.", "This version is already installed.");
                    return update ? Ui.T("LfsPad güncellendi. LFS’yi açabilirsin.", "LfsPad has been updated. You can start LFS.") :
                                    Ui.T("LfsPad kuruldu. LFS’yi açabilirsin.", "LfsPad is installed. You can start LFS.");
                }
            }
            finally
            {
                try
                {
                    File.Delete(script);
                    File.Delete(common);
                    File.Delete(dll);
                    File.Delete(ini);
                    if (Directory.Exists(tools))
                        Directory.Delete(tools);
                    if (Directory.Exists(staging))
                        Directory.Delete(staging);
                }
                catch (IOException) { }
                catch (UnauthorizedAccessException) { }
            }
        }
    }
}

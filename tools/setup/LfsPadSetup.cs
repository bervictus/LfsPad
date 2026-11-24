using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using Microsoft.Win32;

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
        private static readonly string[] SupportedGameHashes =
        {
            "77606E57885FEC33CDDABDFD2271D2F181C65806C2D7EC0049654BD5487D8A52",
            "B7E3BB23288C907F5060075BCDD9ACCF9B7BB26ED57AD76BBE3A3404DA76E881"
        };

        private enum Page { Welcome, Folder, Working, Finished }

        private readonly TextBox folder = new TextBox();
        private readonly Button browse = new Button { AutoSize = true };
        private readonly Button next = new Button { MinimumSize = new Size(86, 29), AutoSize = true };
        private readonly Button back = new Button { MinimumSize = new Size(86, 29), AutoSize = true };
        private readonly Button cancel = new Button { MinimumSize = new Size(86, 29), AutoSize = true };
        private readonly Button remove = new Button { AutoSize = true };
        private readonly Label heading = new Label { AutoSize = true };
        private readonly Label subtitle = new Label { AutoSize = true };
        private readonly Label folderLabel = new Label { AutoSize = true };
        private readonly Label status = new Label { AutoSize = true, MaximumSize = new Size(518, 0) };
        private readonly ComboBox language = new ComboBox { Width = 113 };
        private readonly LinkLabel license = new LinkLabel { AutoSize = true, Anchor = AnchorStyles.Left };
        private readonly ProgressBar progress = new ProgressBar { Style = ProgressBarStyle.Marquee, Dock = DockStyle.Fill };
        private readonly TableLayoutPanel folderControls;
        private Page page;
        private bool searching = true;
        private string detectedFolder;

        internal SetupWindow()
        {
            Font = new Font("Segoe UI", 9);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(568, 341);
            BackColor = Color.White;
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = MinimizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;

            var layout = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 3 };
            layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 93));
            layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 58));
            Controls.Add(layout);
            var header = new TableLayoutPanel
            {
                ColumnCount = 2, Dock = DockStyle.Fill, Padding = new Padding(22, 15, 22, 9),
                Margin = Padding.Empty, BackColor = Color.FromArgb(28, 44, 60), ForeColor = Color.White
            };
            header.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            header.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            layout.Controls.Add(header, 0, 0);
            heading.Font = new Font(Font.FontFamily, 17, FontStyle.Bold);
            heading.Margin = Padding.Empty;
            subtitle.ForeColor = Color.FromArgb(198, 220, 232);
            subtitle.Margin = Padding.Empty;
            header.Controls.Add(heading, 0, 0);
            header.Controls.Add(subtitle, 0, 1);
            language.DropDownStyle = ComboBoxStyle.DropDownList;
            language.Items.AddRange(new object[] { "Türkçe", "English" });
            language.SelectedIndex = Ui.English ? 1 : 0;
            language.AccessibleName = "Dil / Language";
            language.Anchor = AnchorStyles.Right;
            header.Controls.Add(language, 1, 0);
            header.SetRowSpan(language, 2);

            var content = new TableLayoutPanel
            {
                Dock = DockStyle.Fill, ColumnCount = 1, Padding = new Padding(22, 24, 22, 0),
                Margin = Padding.Empty
            };
            layout.Controls.Add(content, 0, 1);
            folderControls = new TableLayoutPanel { AutoSize = true, ColumnCount = 2, Dock = DockStyle.Top, Margin = Padding.Empty };
            folderControls.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            folderControls.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            content.Controls.Add(folderControls, 0, 0);
            folderControls.Controls.Add(folderLabel, 0, 0);
            folderControls.SetColumnSpan(folderLabel, 2);
            folder.Dock = DockStyle.Fill;
            folderControls.Controls.Add(folder, 0, 1);
            folderControls.Controls.Add(browse, 1, 1);
            content.Controls.Add(status, 0, 1);
            status.Margin = new Padding(3, 12, 3, 12);
            content.Controls.Add(progress, 0, 2);
            content.Controls.Add(remove, 0, 3);

            var footer = new TableLayoutPanel
            {
                ColumnCount = 2, Dock = DockStyle.Fill, Margin = Padding.Empty,
                Padding = new Padding(22, 9, 16, 7), BackColor = Color.FromArgb(242, 244, 247)
            };
            footer.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            footer.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            layout.Controls.Add(footer, 0, 2);
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
            buttons.Controls.Add(cancel);
            buttons.Controls.Add(next);
            buttons.Controls.Add(back);
            AcceptButton = next;
            CancelButton = cancel;

            language.SelectedIndexChanged += delegate {
                Ui.English = language.SelectedIndex == 1;
                RefreshActions();
            };
            browse.Click += delegate { ChooseFolder(); };
            next.Click += async delegate {
                if (page == Page.Welcome) {
                    page = Page.Folder;
                    RefreshActions();
                    folder.Focus();
                } else if (page == Page.Folder) {
                    await Apply(false);
                } else if (page == Page.Finished) {
                    Close();
                }
            };
            back.Click += delegate { page = Page.Welcome; RefreshActions(); };
            cancel.Click += delegate { Close(); };
            remove.Click += async delegate { await Apply(true); };
            folder.TextChanged += delegate { RefreshActions(); };
            license.LinkClicked += delegate { ShowLicenses(); };
            FormClosing += delegate(object sender, FormClosingEventArgs e) {
                if (page == Page.Working && e.CloseReason == CloseReason.UserClosing)
                    e.Cancel = true;
            };
            Shown += async delegate {
                detectedFolder = await Task.Run(() => FindGameFolder(Path.GetDirectoryName(Application.ExecutablePath)));
                if (IsDisposed)
                    return;
                searching = false;
                if (folder.Text.Length == 0 && detectedFolder != null)
                    folder.Text = detectedFolder;
                RefreshActions();
            };
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

        private static string FindGameFolder(string installerDirectory)
        {
            var paths = new List<string>();
            foreach (var game in Process.GetProcessesByName("LFS"))
                using (game)
                    try { paths.Add(Path.GetDirectoryName(game.MainModule.FileName)); }
                    catch (System.ComponentModel.Win32Exception) { }
                    catch (InvalidOperationException) { }

            paths.Add(Environment.CurrentDirectory);
            paths.Add(installerDirectory);
            foreach (var view in new[] { RegistryView.Registry64, RegistryView.Registry32 })
                foreach (var hive in new[] { RegistryHive.CurrentUser, RegistryHive.LocalMachine })
                    AddRegisteredFolders(paths, hive, view);

            paths.Add(Path.Combine(Path.GetPathRoot(Environment.SystemDirectory), "LFS"));
            foreach (var folderType in new[] { Environment.SpecialFolder.ProgramFiles, Environment.SpecialFolder.ProgramFilesX86,
                                               Environment.SpecialFolder.LocalApplicationData, Environment.SpecialFolder.MyDocuments,
                                               Environment.SpecialFolder.DesktopDirectory, Environment.SpecialFolder.UserProfile })
            {
                var parent = Environment.GetFolderPath(folderType);
                if (parent.Length == 0)
                    continue;
                paths.Add(Path.Combine(parent, "LFS"));
                paths.Add(Path.Combine(parent, "Live for Speed"));
            }
            foreach (var drive in DriveInfo.GetDrives())
            {
                if (drive.DriveType != DriveType.Fixed)
                    continue;
                foreach (var relative in new[] { "LFS", "Live for Speed", @"Games\LFS", @"Games\Live for Speed" })
                    paths.Add(Path.Combine(drive.Name, relative));
            }
            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            using (var sha = SHA256.Create())
                foreach (var candidate in paths)
                {
                    if (!HasGame(candidate))
                        continue;
                    try
                    {
                        var full = Path.GetFullPath(candidate);
                        if (!seen.Add(full))
                            continue;
                        using (var exe = File.Open(Path.Combine(full, "LFS.exe"), FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
                            if (Array.IndexOf(SupportedGameHashes,
                                    BitConverter.ToString(sha.ComputeHash(exe)).Replace("-", "")) >= 0)
                                return full;
                    }
                    catch (IOException) { }
                    catch (UnauthorizedAccessException) { }
                }
            return null;
        }

        private static void AddRegisteredFolders(List<string> paths, RegistryHive hive, RegistryView view)
        {
            try
            {
                using (var root = RegistryKey.OpenBaseKey(hive, view))
                using (var app = root.OpenSubKey(@"SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\LFS.exe"))
                using (var uninstall = root.OpenSubKey(@"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall"))
                {
                    if (app != null)
                        AddExecutableFolder(paths, app.GetValue(null) as string);
                    if (uninstall == null)
                        return;
                    foreach (var name in uninstall.GetSubKeyNames())
                        using (var key = uninstall.OpenSubKey(name))
                        {
                            var displayName = key == null ? null : key.GetValue("DisplayName") as string;
                            if (displayName == null || !displayName.StartsWith("Live for Speed", StringComparison.OrdinalIgnoreCase))
                                continue;
                            paths.Add(key.GetValue("InstallLocation") as string);
                            AddExecutableFolder(paths, key.GetValue("DisplayIcon") as string);
                            AddExecutableFolder(paths, key.GetValue("UninstallString") as string);
                        }
                }
            }
            catch (IOException) { }
            catch (UnauthorizedAccessException) { }
            catch (System.Security.SecurityException) { }
        }

        private static void AddExecutableFolder(List<string> paths, string command)
        {
            if (String.IsNullOrWhiteSpace(command))
                return;
            command = command.Trim();
            int end = command.IndexOf(".exe", StringComparison.OrdinalIgnoreCase);
            if (end < 0)
                return;
            try { paths.Add(Path.GetDirectoryName(command.Substring(0, end + 4).TrimStart('"'))); }
            catch (ArgumentException) { }
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
            Text = Ui.T("LfsPad Kurulumu", "LfsPad Setup");
            subtitle.Text = "LfsPad " + Application.ProductVersion + "  •  Live for Speed 0.8C38 / 0.8C39";
            folder.AccessibleName = folderLabel.Text = Ui.T("Oyun klasörü", "Game folder");
            browse.Text = Ui.T("Göz at…", "Browse…");
            remove.Text = Ui.T("LfsPad’i kaldır…", "Uninstall LfsPad…");
            license.Text = Ui.T("Üçüncü taraf lisansları", "Third-party licenses");
            back.Text = Ui.T("Geri", "Back");
            cancel.Text = Ui.T("İptal", "Cancel");
            string root = folder.Text.Trim();
            bool valid = HasGame(root);
            bool installed = valid && HasInstallation(root);
            folderControls.Visible = page == Page.Folder;
            back.Visible = page == Page.Folder;
            remove.Visible = page == Page.Folder && installed;
            progress.Visible = page == Page.Working;
            next.Visible = page != Page.Working;
            cancel.Visible = page != Page.Finished;
            cancel.Enabled = page != Page.Working;
            language.Enabled = page == Page.Welcome || page == Page.Folder;
            next.Enabled = page != Page.Folder || valid;
            switch (page)
            {
                case Page.Welcome:
                    heading.Text = Ui.T("Kuruluma hoş geldin", "Welcome to Setup");
                    next.Text = Ui.T("İleri", "Next");
                    status.Text = Ui.T("LfsPad, Live for Speed menülerini kontrolcüyle kullanmanı sağlar. Devam etmeden önce oyunu kapat.",
                        "LfsPad adds controller navigation to Live for Speed menus. Close the game before continuing.");
                    break;
                case Page.Folder:
                    heading.Text = Ui.T("Live for Speed klasörünü seç", "Choose the Live for Speed folder");
                    next.Text = installed ? Ui.T("Güncelle", "Update") : Ui.T("Kur", "Install");
                    status.Text = valid
                        ? (String.Equals(root, detectedFolder, StringComparison.OrdinalIgnoreCase)
                            ? Ui.T("LFS otomatik olarak bulundu. İstersen başka bir klasör seçebilirsin.", "Setup found LFS. You can choose a different folder.")
                            : Ui.T("Klasör kullanıma hazır.", "The folder is ready."))
                        : (searching ? Ui.T("LFS aranıyor…", "Looking for LFS…")
                            : Ui.T("LFS.exe’nin bulunduğu klasörü seç.", "Choose the folder containing LFS.exe."));
                    break;
                case Page.Working:
                    heading.Text = Ui.T("İşlem devam ediyor", "Working on your installation");
                    break;
                case Page.Finished:
                    heading.Text = Ui.T("İşlem tamamlandı", "Setup complete");
                    next.Text = Ui.T("Bitir", "Finish");
                    break;
            }
        }

        private async Task Apply(bool uninstall)
        {
            if (page != Page.Folder)
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
                page = Page.Working;
                RefreshActions();
                status.Text = uninstall ? Ui.T("LfsPad kaldırılıyor…", "Uninstalling LfsPad…") : HasInstallation(root) ? Ui.T("LfsPad güncelleniyor…", "Updating LfsPad…") : Ui.T("LfsPad kuruluyor…", "Installing LfsPad…");
                UseWaitCursor = true;
                string result = await Task.Run(() => RunOperation(root, uninstall));
                page = Page.Finished;
                RefreshActions();
                status.Text = result;
                next.Focus();
            }
            catch (Exception exception)
            {
                page = Page.Folder;
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
            string worker = Path.Combine(tools, "update.ps1");
            string dll = Path.Combine(staging, "dxgi.dll");
            string ini = Path.Combine(staging, "LfsPad.ini");
            try
            {
                Directory.CreateDirectory(tools);
                Extract("install-common.ps1", common);
                Extract(scriptName, script);
                if (!uninstall)
                {
                    Extract("update.ps1", worker);
                    Extract("dxgi.dll", dll);
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
                        else if (detail.Contains("Only verified LFS builds") ||
                                 detail.Contains("Only the verified 0.8C38"))
                            detail = Ui.T("Bu klasörde doğrulanmamış bir LFS sürümü var. Desteklenen bir LFS sürümünün kurulu olduğu klasörü seç.", "Select a folder containing a supported LFS build.");
                        else if (detail.Contains("Invalid InSim port in autoexec.lfs"))
                            detail = Ui.T("autoexec.lfs dosyasındaki InSim portu geçersiz. /insim komutunda 1 ile 65535 arasında bir port kullan.", "The InSim port in autoexec.lfs is invalid. Use a port from 1 to 65535 after /insim.");
                        else if (detail.Contains("Another dxgi.dll is already installed"))
                            detail = Ui.T("Oyun klasöründe başka bir moda ait dxgi.dll var. Kurulum bu dosyayı değiştirmedi.", "The game folder already contains a dxgi.dll from another mod. Setup left that file in place.");
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
                    File.Delete(worker);
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

using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Xml.Linq;
using CameraCaptureUI;
using static System.Windows.Forms.VisualStyles.VisualStyleElement.ProgressBar;

namespace CameraCaptureUI
{
    public struct CameraListStruct
    {
        public int count;
        public IntPtr names; // Representa el wchar_t** de C++
    }
    public struct CameraFormat
    {
        public uint Width;
        public uint Height;
        public uint FpsNumerator;
        public uint FpsDenominator;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] // Make sure SizeConst is large enough!
        public string Subtype;
    }
    public partial class Form1 : Form
    {
        private IntPtr session1 = IntPtr.Zero;
        private IntPtr session2 = IntPtr.Zero;
        bool isRecording = false;
        // bool isPause = false;
        private System.Windows.Forms.Timer delayTimer;
        private int delaySeconds = 10;
        private List<Button> delayedButtons = new List<Button>();
        private string selectedNameCam1 = "";
        private string selectedNameCam2 = "";
        private bool previewCam1 = false;
        private bool previewCam2 = false;


        private CameraInterop.ErrorCallback errorCallbackDelegate;

        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool SetDllDirectory(string lpPathName);
        public Form1()
        {
            InitializeComponent();
            string path = System.IO.Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location);
            SetDllDirectory(path);
            checkBoxCamera2.Checked = false;
            delayTimer = new System.Windows.Forms.Timer();
            delayTimer.Interval = delaySeconds * 500;
            delayTimer.Tick += DelayTimer_Tick;
        }
        private void EnableButtonsAfterDelay(List<Button> buttons, int delayInSeconds = 10)
        {
            // Deshabilita los botones
            foreach (var btn in buttons)
            {
                btn.Enabled = false;
            }

            // Guarda los botones a reactivar
            delayedButtons = buttons;

            // Configura y arranca el timer
            delayTimer.Interval = delayInSeconds * 500; // 9 segundos
            delayTimer.Start();
        }
        private void DelayTimer_Tick(object sender, EventArgs e)
        {
            // Habilita los botones previamente guardados
            foreach (var btn in delayedButtons)
            {
                btn.Enabled = true;
            }

            // Limpia la lista y detiene el timer
            delayedButtons.Clear();
            delayTimer.Stop();
        }

        private async void Form1_Load(object sender, EventArgs e)
        {
            CameraInterop.InitializeCameraSystem();
            errorCallbackDelegate = ShowError;
            CameraInterop.SetErrorCallback(errorCallbackDelegate);
            LoadBitrate();
            LoadCameras();
            LoadMicrophones();
            comboBoxCameras2.Enabled = false;
            btnStop.Enabled = false;
            comboBoxVideoFormatsCam1.Items.Clear();
            comboBoxVideoFormatsCam2.Items.Clear();
            comboBoxVideoFormatsCam1.Text = "Seleccione una opción...";
            comboBoxVideoFormatsCam2.Text = "Seleccione una opción...";

            //btnPause.Enabled = false;
            //btnContinue.Enabled = false;
        }
        protected override void OnFormClosed(FormClosedEventArgs e)
        {
            CameraInterop.ShutdownCameraSystem();
            base.OnFormClosed(e);
        }

        private void LoadBitrate()
        {
            var bitrateOptions = new Dictionary<string, int>
            {
                { "500 kbps (baja calidad)", 500000 },
                { "1 Mbps (media calidad)", 1000000 },
                { "2 Mbps (alta calidad)", 2000000 },
                { "4 Mbps (muy alta calidad)", 4000000 },
                { "8 Mbps (ultra calidad)", 8000000 }
            };

            comboBoxBitrates.DisplayMember = "Key";
            comboBoxBitrates.ValueMember = "Value";
            comboBoxBitrates.DataSource = new BindingSource(bitrateOptions, null);
            comboBoxBitrates.SelectedIndex = 0;
        }

        private void LoadCameras()
        {
            comboBoxCameras1.Items.Clear();
            comboBoxCameras1.Text = "Seleccione una opción...";
            comboBoxCameras2.Items.Clear();
            comboBoxCameras2.Text = "Seleccione una opción...";
            if(!previewCam1)
            {
                comboBoxVideoFormatsCam1.Items.Clear();
                comboBoxVideoFormatsCam1.Text = "Seleccione una opción...";
            }
            if (!previewCam2)
            {
                comboBoxVideoFormatsCam2.Items.Clear();
                comboBoxVideoFormatsCam2.Text = "Seleccione una opción...";
            }


            int count = CameraInterop.GetCameraCount();
            if (count > 0)
            {
                for (int i = 0; i < count; i++)
                {
                    char[] buffer = new char[256];
                    CameraInterop.GetCameraName(i, buffer, buffer.Length);
                    string name = new string(buffer).TrimEnd('\0');
                    if (name != "")
                    {
                        comboBoxCameras1.Items.Add(new CameraItem { Name = name, Index = i });
                        comboBoxCameras2.Items.Add(new CameraItem { Name = name, Index = i });
                    }
                }
            }
   

            // Maneja el evento CheckedChanged del CheckBox
            checkBoxCamera2.CheckedChanged += checkBox1_CheckedChanged;
            //if (comboBoxCameras.Items.Count > 0) comboBoxCameras.SelectedIndex = 0;
            //if (comboBoxCameras2.Items.Count > 1) comboBoxCameras2.SelectedIndex = 1;
            //else if (comboBoxCameras2.Items.Count > 0) comboBoxCameras2.SelectedIndex = 0;
        }

        private void LoadMicrophones()
        {
            comboBoxMicrophones.Items.Clear();
            comboBoxMicrophones.Text = "Seleccione una opción...";

            int count = CameraInterop.GetMicrophoneCount();
            for (int i = 0; i < count; i++)
            {
                char[] buffer = new char[256];
                CameraInterop.GetMicrophoneName(i, buffer, buffer.Length);
                string name = new string(buffer).TrimEnd('\0');
                comboBoxMicrophones.Items.Add(new CameraItem { Name = name, Index = i });
            }

            if (comboBoxMicrophones.Items.Count > 0) comboBoxMicrophones.SelectedIndex = 0;
        }

        private void LoadCameraFormats(string friendlyName, int Cam)
        {
            if (!previewCam1)
            {
                comboBoxVideoFormatsCam1.Items.Clear();
                comboBoxVideoFormatsCam1.Text = "Seleccione una opción...";
                
            }
            if (!previewCam2)
            {
                comboBoxVideoFormatsCam2.Items.Clear();
                comboBoxVideoFormatsCam2.Text = "Seleccione una opción...";
            }

            if (CameraInterop.GetSupportedFormats(friendlyName, out IntPtr ptr, out int count))
            {
                int size = Marshal.SizeOf<CameraFormat>();
                var uniqueDescriptions = new HashSet<string>();

                for (int i = 0; i < count; i++)
                {
                    IntPtr itemPtr = IntPtr.Add(ptr, i * size);
                    CameraFormat format = Marshal.PtrToStructure<CameraFormat>(itemPtr);

                    string description = $"{format.Width}x{format.Height} / {(format.FpsNumerator / (double)format.FpsDenominator):F0} fps / {format.Subtype}";
                    string key = $"{format.Width}x{format.Height},{format.FpsNumerator}{format.FpsDenominator}{format.Subtype}";

                    if (uniqueDescriptions.Add(key))
                    {
                        if (Cam == 1)
                        {
                            comboBoxVideoFormatsCam1.Items.Add(new CameraFormatItem
                            {
                                Index = i,               // Este es el índice real en el dispositivo
                                Description = description
                            });

                        } else if (Cam == 2)
                        {
                            comboBoxVideoFormatsCam2.Items.Add(new CameraFormatItem
                            {
                                Index = i,               // Este es el índice real en el dispositivo
                                Description = description
                            });
                        }
                    }
                }
                if (Cam == 1 && comboBoxVideoFormatsCam1.Items.Count > 0) comboBoxVideoFormatsCam1.SelectedIndex = 0;
                if (Cam == 2 && comboBoxVideoFormatsCam1.Items.Count > 0) comboBoxVideoFormatsCam2.SelectedIndex = 0;
                CameraInterop.FreeFormats(ptr);
            }
        }

        private void startRecording_Click(object sender, EventArgs e)
        {
            string cameraFriendlyName = null;
            string cameraFriendlyName2 = null;
            string micFriendlyName = null;

            if (!isRecording)
            {
                if (comboBoxCameras1.SelectedItem is CameraItem selectedCamera)
                {
                    cameraFriendlyName = selectedCamera.Name;
                }
                else
                {
                    MessageBox.Show("Por favor, seleccione la cámara 1 del desplegable.");
                    return;
                }
                if (checkBoxCamera2.Checked)
                {
                    if (comboBoxCameras2.SelectedItem is CameraItem selectedCamera2)
                    {
                        cameraFriendlyName2 = selectedCamera2.Name;
                    }
                    else
                    {
                        MessageBox.Show("Por favor, seleccione la cámara 2 del desplegable.");
                        return;
                    }
                }

                if (comboBoxMicrophones.SelectedItem is CameraItem selectedMic)
                {
                    micFriendlyName = selectedMic.Name;
                }
                else
                {
                    MessageBox.Show("Por favor, seleccione el microfono del desplegable.");
                    return;
                }

                if (cameraFriendlyName != cameraFriendlyName2)
                {
                    if (comboBoxVideoFormatsCam1.SelectedItem is CameraFormatItem selectedFormat)
                    {
                        int bitrate = (int)((KeyValuePair<string, int>)comboBoxBitrates.SelectedItem).Value;
                        if (bitrate > 0)
                        {
                            using (var saveDialog = new SaveFileDialog())
                            {
                                saveDialog.Filter = "MP4 files (*.mp4)|*.mp4";
                                saveDialog.Title = "Guardar video";
                                if (saveDialog.ShowDialog() == DialogResult.OK)
                                {
                                    if (cameraFriendlyName != null && cameraFriendlyName2 != null)
                                    {
                                        isRecording = CameraInterop.StartRecordingTwoCameras(cameraFriendlyName, cameraFriendlyName2, micFriendlyName, saveDialog.FileName, bitrate);
                                        if (isRecording)
                                        {
                                            labelStatusCam1.Text = "Camara 1 grabando...";
                                            labelStatusCam2.Text = "Camara 2 grabando...";
                                        }
                                    }
                                    else
                                    {
                                        isRecording = CameraInterop.StartRecording(cameraFriendlyName, micFriendlyName, saveDialog.FileName, selectedFormat.Index, bitrate);
                                        labelStatusCam1.Text = "Camara 1 grabando...";
                                    }
                                    if (isRecording)
                                    {
                                        btnStart.Enabled = false;
                                        // btnPreview.Enabled = false;
                                        EnableButtonsAfterDelay(new List<Button> { btnStop }, 10);
                                    }
                                }
                            }
                        }
                        else
                        {
                            MessageBox.Show("Por favor, seleccione el Bitrate en el desplegable.");
                        }
                    }
                    else
                    {
                        MessageBox.Show("Por favor, seleccione un formato en el desplegable.");
                    }
                }
                else
                {
                    MessageBox.Show("Por favor, seleccione camaras diferentes.");
                }
            }
            else
            {
                MessageBox.Show("Grabacion en proceso, detenga la grabacion para empezar una nueva.");
            }

        }

        //private void pauseRecording_Click(object sender, EventArgs e)
        //{
        //    if (isRecording)
        //    {
        //        if (!isPause)
        //        {
        //            isPause = CameraInterop.PauseRecording();
        //            if (!isPause) MessageBox.Show("Espera unos segundos mientras la grabacion empieza.");
        //            if (isPause)
        //            {
        //                EnableButtonsAfterDelay(new List<Button> { btnContinue }, 10);
        //                btnPause.Enabled = false;
        //                if (checkBoxCamera2.Checked)
        //                {
        //                    labelStatusCam1.Text = "Camara 1 en pausa...";
        //                    labelStatusCam2.Text = "Camara 2 en pausa...";
        //                }
        //                else
        //                {
        //                    labelStatusCam1.Text = "Camara 1 en pausa...";
        //                }
        //            }
        //        }
        //        else
        //        {
        //            MessageBox.Show("Grabacion ya esta pausada.");
        //        }
        //    }
        //    else
        //    {
        //        MessageBox.Show("No hay grabacion en proceso, por favor inicie una grabacion.");
        //    }
        //}

        //private void continueRecording_Click(object sender, EventArgs e)
        //{
        //    if (isRecording)
        //    {
        //        if (isPause)
        //        {
        //            bool continueRecording = CameraInterop.ContinueRecording();
        //            if (continueRecording)
        //            {
        //                EnableButtonsAfterDelay(new List<Button> { btnPause }, 10);
        //                isPause = false;
        //                btnContinue.Enabled = false;
        //                if (checkBoxCamera2.Checked)
        //                {
        //                    labelStatusCam1.Text = "Camara 1 grabando...";
        //                    labelStatusCam2.Text = "Camara 2 grabando...";
        //                }
        //                else
        //                {
        //                    labelStatusCam1.Text = "Camara 1 grabando...";
        //                }
        //            }
        //        }
        //        else
        //        {
        //            MessageBox.Show("La grabacion no esta en pausa porfavor pause primero.");
        //        }
        //    }
        //    else
        //    {
        //        MessageBox.Show("No hay grabacion en proceso, por favor inicie una.");
        //    }
        //}

        private void stopRecording_Click(object sender, EventArgs e)
        {
            bool stopRecording = CameraInterop.StopRecording();
            if (stopRecording)
            {
                EnableButtonsAfterDelay(new List<Button> { btnStart }, 6);
                isRecording = false;
                btnStop.Enabled = false;
                //btnPause.Enabled = false;
                //btnContinue.Enabled = false;
                // btnPreview.Enabled = true;
                if (checkBoxCamera2.Checked)
                {
                    labelStatusCam1.Text = "Camara 1 Finalizo";
                    labelStatusCam2.Text = "Camara 2 Finalizo";
                }
                else
                {
                    labelStatusCam1.Text = "Camara 1 Finalizo";
                }
            }
        }

        private void ShowError(string message)
        {
            MessageBox.Show(message, "Error de grabación", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }

        private void checkBox1_CheckedChanged(object sender, EventArgs e)
        {
            comboBoxCameras2.Enabled = checkBoxCamera2.Checked;
        }

        private void btnTakePhotoCam1_Click(object sender, EventArgs e)
        {
            if (comboBoxCameras1.SelectedItem is CameraItem selectedPhoto)
            {
                using (var saveDialog = new SaveFileDialog())
                {
                    saveDialog.Title = "Guardar imagen";
                    saveDialog.Filter = "Imagen JPEG (*.jpg)|*.jpg|Imagen PNG (*.png)|*.png|Imagen BMP (*.bmp)|*.bmp";
                    saveDialog.DefaultExt = "jpg";
                    saveDialog.OverwritePrompt = true;

                    if (saveDialog.ShowDialog() == DialogResult.OK)
                    {
                        string fileName = saveDialog.FileName;

                        if (string.IsNullOrEmpty(Path.GetExtension(fileName)))
                        {
                            switch (saveDialog.FilterIndex)
                            {
                                case 1: fileName += ".jpg"; break;
                                case 2: fileName += ".png"; break;
                                case 3: fileName += ".bmp"; break;
                            }
                        }

                        string cameraFriendlyName = selectedPhoto.Name;

                        bool success = CameraInterop.CaptureSnapshott(cameraFriendlyName, fileName);
                        if (success) MessageBox.Show(fileName, "Foto guardad en:");
                        else MessageBox.Show("La toma de la foto falló");
                    }
                }
            }
            else
            {
                MessageBox.Show("Por favor seleccione la camara 1 en el desplegable.");
            }
        }

        private void btnTakePhotoCam2_Click(object sender, EventArgs e)
        {
            if (checkBoxCamera2.Checked)
            {
                if (comboBoxCameras2.SelectedItem is CameraItem selectedPhoto)
                {
                    using (var saveDialog = new SaveFileDialog())
                    {
                        saveDialog.Title = "Guardar imagen";
                        saveDialog.Filter = "Imagen JPEG (*.jpg)|*.jpg|Imagen PNG (*.png)|*.png|Imagen BMP (*.bmp)|*.bmp";
                        saveDialog.DefaultExt = "jpg";
                        saveDialog.OverwritePrompt = true;

                        if (saveDialog.ShowDialog() == DialogResult.OK)
                        {
                            string fileName = saveDialog.FileName;

                            if (string.IsNullOrEmpty(Path.GetExtension(fileName)))
                            {
                                switch (saveDialog.FilterIndex)
                                {
                                    case 1: fileName += ".jpg"; break;
                                    case 2: fileName += ".png"; break;
                                    case 3: fileName += ".bmp"; break;
                                }
                            }

                            string cameraFriendlyName = selectedPhoto.Name;

                            bool success = CameraInterop.CaptureSnapshott(cameraFriendlyName, fileName);
                            if (success) MessageBox.Show(fileName, "Foto guardad en:");
                            else MessageBox.Show("La toma de la foto falló");
                        }
                    }
                }
                else
                {
                    MessageBox.Show("Por favor seleccione la camara 2 en el desplegable.");
                }
            }
            else
            {
                MessageBox.Show("Por favor habilite la camara 2.");
            }

        }

        private void comboBoxCameras1_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (comboBoxCameras1.SelectedItem is CameraItem selected1)
            {
                if (selectedNameCam2 != selected1.Name) // CORRECTO
                {
                    pictureBoxPreview.Image = null;
                    pictureBoxPreview.Invalidate();
                    stopPreview(selectedNameCam1);
                    IntPtr hwnd1 = pictureBoxPreview.Handle;
                    previewCam1 = CameraInterop.StartPreview(selected1.Name, hwnd1);
                    if (previewCam1)
                    {
                        LoadCameraFormats(selected1.Name, 1);
                        selectedNameCam1 = selected1.Name;
                    }
                }
                else
                {
                    MessageBox.Show("En camara 1 seleccione una camara diferente a la camara 2");
                }
            }
            else
            {
                MessageBox.Show("Seleccione Camara 1 del desplegable");
            }
        }

        private void comboBoxCameras2_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (checkBoxCamera2.Checked)
            {
                if (comboBoxCameras2.SelectedItem is CameraItem selected2)
                {
                    if (selectedNameCam1 != selected2.Name) // CORRECTO
                    {
                        pictureBoxPreview2.Image = null;
                        pictureBoxPreview2.Invalidate();
                        stopPreview(selectedNameCam2);
                        IntPtr hwnd2 = pictureBoxPreview2.Handle;
                        previewCam2 = CameraInterop.StartPreview(selected2.Name, hwnd2);
                        if (previewCam2)
                        {
                            LoadCameraFormats(selected2.Name, 2);
                            selectedNameCam2 = selected2.Name;
                        }
                    }
                    else
                    {
                        MessageBox.Show("En camara 2 seleccione una camara diferente a la camara 1");
                    }
                }
                else
                {
                    MessageBox.Show("Seleccione Camara 2 del desplegable");
                }
            }
        }

        protected override void WndProc(ref Message m)
        {
            const int WM_DEVICECHANGE = 0x0219;
            const int DBT_DEVNODES_CHANGED = 0x0007;

            if (m.Msg == WM_DEVICECHANGE && (int)m.WParam == DBT_DEVNODES_CHANGED)
            {
                LoadCameras();
            }

            base.WndProc(ref m);
        }

        private void stopPreview(string cameraFriendlyName)
        {
            CameraInterop.StopPreview(cameraFriendlyName);
        }

    }
}
public struct CameraList
{
    public int count;
    public IntPtr names;
}

public static class CameraInterop
{
    public delegate void ErrorCallback([MarshalAs(UnmanagedType.LPWStr)] string message);

    [DllImport("CameraCaptureLib.dll")]
    public static extern void SetErrorCallback(ErrorCallback callback);

    [DllImport("CameraCaptureLib.dll", CharSet = CharSet.Unicode)]
    public static extern void InitializeCameraSystem();

    [DllImport("CameraCaptureLib.dll")]
    public static extern void ShutdownCameraSystem();

    [DllImport("CameraCaptureLib.dll")]
    public static extern int GetCameraCount();

    [DllImport("CameraCaptureLib.dll", CharSet = CharSet.Unicode)]
    public static extern void GetCameraName(int index, [Out] char[] nameBuffer, int bufferLength);

    [DllImport("CameraCaptureLib.dll", CharSet = CharSet.Unicode)]
    public static extern bool StartPreview(string cameraFriendlyName, IntPtr hwnd);

    [DllImport("CameraCaptureLib.dll", CharSet = CharSet.Unicode)]
    public static extern void StopPreview(string cameraFriendlyName);

    [DllImport("CameraCaptureLib.dll", CharSet = CharSet.Unicode)]
    public static extern int GetMicrophoneCount();

    [DllImport("CameraCaptureLib.dll", CharSet = CharSet.Unicode)]
    public static extern void GetMicrophoneName(int index, [Out] char[] nameBuffer, int nameBufferSize);

    [DllImport("CameraCaptureLib.dll", CharSet = CharSet.Unicode)]
    public static extern bool StartRecording(string cameraFriendlyName, string micFriendlyName, string outputPath, int indexFormat,int bitrate);

    [DllImport("CameraCaptureLib.dll", CharSet = CharSet.Unicode)]
    public static extern bool StartRecordingTwoCameras(string cameraFriendlyName, string cameraFriendlyName2, string micFriendlyName, string outputPath, int bitrate);

    [DllImport("CameraCaptureLib.dll")]
    public static extern bool PauseRecording();

    [DllImport("CameraCaptureLib.dll")]
    public static extern bool ContinueRecording();

    [DllImport("CameraCaptureLib.dll")]
    public static extern bool StopRecording();

    [DllImport("CameraCaptureLib.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
    public static extern bool GetSupportedFormats([MarshalAs(UnmanagedType.LPWStr)] string friendlyName, out IntPtr formats, out int count);

    [DllImport("CameraCaptureLib.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void FreeFormats(IntPtr formats);

    [DllImport("CameraCaptureLib.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
    public static extern bool CaptureSnapshott(string cameraFriendlyName, string filename);
}

public struct PreviewSession
{
    public IntPtr pSession;
    public IntPtr pSource;
    public IntPtr pTopology;
    public IntPtr hwndPreview;
}

public struct CameraFormat
{
    public uint Width;
    public uint Height;
    public uint FpsNumerator;
    public uint FpsDenominator;
}

public class CameraItem
{
    public string Name { get; set; }
    public int Index { get; set; }
    public override string ToString() => Name; // Para mostrar en el ComboBox
}

class CameraFormatItem
{
    public int Index { get; set; }
    public string Description { get; set; }
    public override string ToString() => Description;
}
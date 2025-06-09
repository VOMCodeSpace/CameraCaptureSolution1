using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace CameraCaptureUI
{
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)] // Use Ansi for char[]
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
        PreviewSession session1;
        PreviewSession session2;
        bool isRecording = false;
        bool isRecordingTwoCameras = false;
        bool isPause = false;
        private System.Windows.Forms.Timer delayTimer;
        private int delaySeconds = 10;
        private List<Button> delayedButtons = new List<Button>();


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
            delayTimer.Interval = delaySeconds * 900; // 9 segundos
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
            delayTimer.Interval = delayInSeconds * 900;
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

        private void Form1_Load(object sender, EventArgs e)
        {
            CameraInterop.InitializeCameraSystem();

            errorCallbackDelegate = ShowError;
            CameraInterop.SetErrorCallback(errorCallbackDelegate);
            comboBoxCameras2.Enabled = false;
            LoadCameras();
            LoadMicrophones();
            btnPause.Enabled = false;
            btnContinue.Enabled = false;
            btnStop.Enabled = false;
        }
        protected override void OnFormClosed(FormClosedEventArgs e)
        {
            CameraInterop.ShutdownCameraSystem();
            base.OnFormClosed(e);
        }

        private void LoadCameras()
        {
            comboBoxCameras.Items.Clear();
            comboBoxCameras2.Items.Clear();
            comboBoxCameras.Text = "Seleccione una opción...";
            comboBoxCameras2.Text = "Seleccione una opción...";

            int count = CameraInterop.GetCameraCount();
            for (int i = 0; i < count; i++)
            {
                char[] buffer = new char[256];
                CameraInterop.GetCameraName(i, buffer, buffer.Length);
                string name = new string(buffer).TrimEnd('\0');
                comboBoxCameras.Items.Add(new CameraItem { Name = name, Index = i });
                comboBoxCameras2.Items.Add(new CameraItem { Name = name, Index = i });
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

            if (comboBoxMicrophones.Items.Count > 0)
                comboBoxMicrophones.SelectedIndex = 0;
        }

        private void LoadCameraFormats(int deviceIndex)
        {
            comboBoxVideoFormats.Items.Clear(); // Clear before loading
            comboBoxVideoFormats.Text = "Seleccione una opción...";

            if (CameraInterop.GetSupportedFormats(deviceIndex, out IntPtr ptr, out int count))
            {
                int size = Marshal.SizeOf<CameraFormat>();
                var uniqueFormats = new HashSet<string>();

                for (int i = 0; i < count; i++)
                {
                    IntPtr itemPtr = IntPtr.Add(ptr, i * size);
                    CameraFormat format = Marshal.PtrToStructure<CameraFormat>(itemPtr);

                    string key = $"{format.Width}x{format.Height}@{format.FpsNumerator}/{format.FpsDenominator} ({format.Subtype})";

                    if (uniqueFormats.Add(key))
                    {
                        string description = $"{format.Width}x{format.Height} @ {(format.FpsNumerator / (double)format.FpsDenominator):F2} fps ({format.Subtype})";
                        comboBoxVideoFormats.Items.Add(description);
                    }
                }

                // This is the correct place to free the memory.
                //CameraInterop.FreeFormats(ptr);

                //if (comboBoxVideoFormats.Items.Count > 0)
                //{
                //    comboBoxVideoFormats.SelectedIndex = 0;
                //}
            }
            // REMOVED: CameraInterop.FreeFormats(ptr); // This line is the problem!
        }

        private void startPreview()
        {
            int camIndex = -1;
            int cam2Index = -1;
            if (comboBoxCameras.SelectedItem is CameraItem selected1)
            {
                CameraInterop.StopPreview(ref session1);
                CameraInterop.StopPreview(ref session2);
                IntPtr hwnd1 = pictureBoxPreview.Handle;
                camIndex = selected1.Index;
                CameraInterop.StartPreview(selected1.Index, hwnd1, ref session1);
            }
            else
            {
                MessageBox.Show("Seleccione Camara 1 del desplegable");
            }
            if (checkBoxCamera2.Checked)
            {
                if (comboBoxCameras2.SelectedItem is CameraItem selected2)
                {
                    cam2Index = selected2.Index;
                    if (camIndex != cam2Index)
                    {
                        CameraInterop.StopPreview(ref session2);
                        IntPtr hwnd2 = pictureBoxPreview2.Handle;
                        CameraInterop.StartPreview(selected2.Index, hwnd2, ref session2);
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


        private void btnPreview_Click(object sender, EventArgs e)
        {
            pictureBoxPreview.Image = null;
            pictureBoxPreview.Invalidate(); // fuerza el repintado visual
            pictureBoxPreview2.Image = null;
            pictureBoxPreview2.Invalidate();
            startPreview();
        }

        private void startRecording_Click(object sender, EventArgs e)
        {
            int camIndex = -1;
            int cam2Index = -1;
            int micIndex = -1;

            if (!isRecording)
            {
                if (comboBoxCameras.SelectedItem is CameraItem selectedCamera)
                {
                    camIndex = selectedCamera.Index;
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
                        cam2Index = selectedCamera2.Index;
                    }
                    else
                    {
                        MessageBox.Show("Por favor, seleccione la cámara 2 del desplegable.");
                        return;
                    }
                }

                if (comboBoxMicrophones.SelectedItem is CameraItem selectedMic)
                {
                    micIndex = selectedMic.Index;
                }
                else
                {
                    MessageBox.Show("Por favor, seleccione el microfono del desplegable.");
                    return;
                }

                if (camIndex != cam2Index)
                {
                    using (var saveDialog = new SaveFileDialog())
                    {
                        saveDialog.Filter = "MP4 files (*.mp4)|*.mp4";
                        saveDialog.Title = "Guardar video";
                        if (saveDialog.ShowDialog() == DialogResult.OK)
                        {
                            if (camIndex >= 0 && cam2Index >= 0)
                            {
                                isRecording = CameraInterop.StartRecordingTwoCameras(camIndex, cam2Index, micIndex, saveDialog.FileName);
                                if (isRecording)
                                {
                                    labelStatusCam1.Text = "Camara 1 grabando...";
                                    labelStatusCam2.Text = "Camara 2 grabando...";
                                }
                            }
                            else
                            {
                                isRecording = CameraInterop.StartRecording(camIndex, micIndex, saveDialog.FileName);
                                labelStatusCam1.Text = "Camara 1 grabando...";
                            }
                            if (isRecording)
                            {
                                btnStart.Enabled = false;
                                EnableButtonsAfterDelay(new List<Button> { btnPause, btnStop }, 10);
                            }
                        }
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

        private void pauseRecording_Click(object sender, EventArgs e)
        {
            if (isRecording)
            {
                if (!isPause)
                {
                    isPause = CameraInterop.PauseRecording();
                    if (!isPause) MessageBox.Show("Espera unos segundos mientras la grabacion empieza.");
                    if (isPause)
                    {
                        EnableButtonsAfterDelay(new List<Button> { btnContinue }, 10);
                        btnPause.Enabled = false;
                        if (checkBoxCamera2.Checked)
                        {
                            labelStatusCam1.Text = "Camara 1 en pausa...";
                            labelStatusCam2.Text = "Camara 2 en pausa...";
                        } else
                        {
                            labelStatusCam1.Text = "Camara 1 en pausa...";
                        }
                    }
                }
                else
                {
                    MessageBox.Show("Grabacion ya esta pausada.");
                }
            }
            else
            {
                MessageBox.Show("No hay grabacion en proceso, por favor inicie una grabacion.");
            }
        }

        private void continueRecording_Click(object sender, EventArgs e)
        {
            if (isRecording)
            {
                if (isPause)
                {
                    bool continueRecording = CameraInterop.ContinueRecording();
                    if (continueRecording)
                    {
                        EnableButtonsAfterDelay(new List<Button> { btnPause }, 10);
                        isPause = false;
                        btnContinue.Enabled = false;
                        if (checkBoxCamera2.Checked)
                        {
                            labelStatusCam1.Text = "Camara 1 grabando...";
                            labelStatusCam2.Text = "Camara 2 grabando...";
                        }
                        else
                        {
                            labelStatusCam1.Text = "Camara 1 grabando...";
                        }
                    }
                }
                else
                {
                    MessageBox.Show("La grabacion no esta en pausa porfavor pause primero.");
                }
            }
            else
            {
                MessageBox.Show("No hay grabacion en proceso, por favor inicie una.");
            }
        }

        private void stopRecording_Click(object sender, EventArgs e)
        {
            bool stopRecording = CameraInterop.StopRecording();
            if (stopRecording)
            {
                EnableButtonsAfterDelay(new List<Button> { btnStart }, 10);
                isRecording = false;
                btnPause.Enabled = false;
                btnContinue.Enabled = false;
                btnStop.Enabled = false;
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

        private void reloadCameras_Click(object sender, EventArgs e)
        {
            LoadCameras();
        }

        private void loadFormats_Click(object sender, EventArgs e)
        {
            if (comboBoxCameras.SelectedItem is CameraItem selected)
            {
                LoadCameraFormats(selected.Index);
            }
            else
            {
                MessageBox.Show("Seleccione Camara 1");
            }

        }
    }
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

    [DllImport("CameraCaptureLib.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern bool StartPreview(int index, IntPtr hwnd, ref PreviewSession session);

    [DllImport("CameraCaptureLib.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void StopPreview(ref PreviewSession session);

    [DllImport("CameraCaptureLib.dll", CharSet = CharSet.Unicode)]
    public static extern int GetMicrophoneCount();

    [DllImport("CameraCaptureLib.dll", CharSet = CharSet.Unicode)]
    public static extern void GetMicrophoneName(int index, [Out] char[] nameBuffer, int nameBufferSize);

    [DllImport("CameraCaptureLib.dll", CharSet = CharSet.Unicode)]
    public static extern bool StartRecording(int videoDeviceIndex, int audioDevice, string outputPath);

    [DllImport("CameraCaptureLib.dll", CharSet = CharSet.Unicode)]
    public static extern bool StartRecordingTwoCameras(int videoDeviceIndex, int videoDevicendex, int audioDevice, string outputPath);

    [DllImport("CameraCaptureLib.dll")]
    public static extern bool PauseRecording();

    [DllImport("CameraCaptureLib.dll")]
    public static extern bool ContinueRecording();

    [DllImport("CameraCaptureLib.dll")]
    public static extern bool StopRecording();

    [DllImport("CameraCaptureLib.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern bool GetSupportedFormats(int deviceIndex, out IntPtr formats, out int count);

    [DllImport("CameraCaptureLib.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void FreeFormats(IntPtr formats);
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
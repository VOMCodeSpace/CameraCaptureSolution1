using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace CameraCaptureUI
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            CameraInterop.InitializeCameraSystem();
            LoadCameras();
            LoadMicrophones();
        }
        protected override void OnFormClosed(FormClosedEventArgs e)
        {
            CameraInterop.ShutdownCameraSystem();
            base.OnFormClosed(e);
        }

        private void LoadCameras ()
        {
            int count = CameraInterop.GetCameraCount();
            for (int i = 0; i < count; i++)
            {
                char[] buffer = new char[256];
                CameraInterop.GetCameraName(i, buffer, buffer.Length);
                string name = new string(buffer).TrimEnd('\0');
                comboBoxCameras.Items.Add(new CameraItem { Name = name, Index = i });
            }

            if (comboBoxCameras.Items.Count > 0)
                comboBoxCameras.SelectedIndex = 0;
        }

        private void LoadMicrophones()
        {
            comboBoxMicrophones.Items.Clear();

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

        private void btnPreview_Click(object sender, EventArgs e)
        {
            if (comboBoxCameras.SelectedItem is CameraItem selected)
            {
                IntPtr hwnd = pictureBoxPreview.Handle;
                CameraInterop.StartPreview(selected.Index, hwnd);
            }
        }

        private void button1_Click(object sender, EventArgs e)
        {
            var camIndex = ((CameraItem)comboBoxCameras.SelectedItem).Index;
            var micIndex = ((CameraItem)comboBoxMicrophones.SelectedItem).Index;


            using (var saveDialog = new SaveFileDialog())
            {
                saveDialog.Filter = "MP4 files (*.mp4)|*.mp4";
                saveDialog.Title = "Guardar video";

                if (saveDialog.ShowDialog() == DialogResult.OK)
                {
                    CameraInterop.StartRecording(camIndex, saveDialog.FileName);
                    
                }
            }
        }

        private void button2_Click(object sender, EventArgs e)
        {
            CameraInterop.PauseRecording();
        }

        private void button3_Click(object sender, EventArgs e)
        {
            CameraInterop.ResumeRecording();
        }

        private void button4_Click(object sender, EventArgs e)
        {
            CameraInterop.StopRecording();
        }
    }
}
public static class CameraInterop
{
    [DllImport("CameraCaptureLib.dll", CharSet = CharSet.Unicode)]
    public static extern void InitializeCameraSystem();

    [DllImport("CameraCaptureLib.dll")]
    public static extern void ShutdownCameraSystem();

    [DllImport("CameraCaptureLib.dll")]
    public static extern int GetCameraCount();

    [DllImport("CameraCaptureLib.dll", CharSet = CharSet.Unicode)]
    public static extern void GetCameraName(int index, [Out] char[] nameBuffer, int bufferLength);

    [DllImport("CameraCaptureLib.dll")]
    public static extern bool StartPreview(int index, IntPtr hwnd);

    [DllImport("CameraCaptureLib.dll")]
    public static extern void StopPreview();

    [DllImport("CameraCaptureLib.dll", CharSet = CharSet.Unicode)]
    public static extern int GetMicrophoneCount();

    [DllImport("CameraCaptureLib.dll", CharSet = CharSet.Unicode)]
    public static extern void GetMicrophoneName(int index, [Out] char[] nameBuffer, int nameBufferSize);

    [DllImport("CameraCaptureLib.dll", CharSet = CharSet.Unicode)]
    public static extern bool StartRecording(int videoDeviceIndex, string outputPath);

    [DllImport("CameraCaptureLib.dll")]
    public static extern bool PauseRecording();

    [DllImport("CameraCaptureLib.dll")]
    public static extern bool ResumeRecording();

    [DllImport("CameraCaptureLib.dll")]
    public static extern bool StopRecording();
}

public class CameraItem
{
    public string Name { get; set; }
    public int Index { get; set; }

    public override string ToString() => Name; // Para mostrar en el ComboBox
}
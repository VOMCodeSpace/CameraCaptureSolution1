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
        PreviewSession session1;
        PreviewSession session2;

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
                comboBoxCameras2.Items.Add(new CameraItem { Name = name, Index = i });
            }

            if (comboBoxCameras.Items.Count > 0)
                comboBoxCameras.SelectedIndex = 0;
            if (comboBoxCameras2.Items.Count > 1)
            {
                comboBoxCameras2.SelectedIndex = 1;
            } else if (comboBoxCameras2.Items.Count > 0)
            {
                comboBoxCameras2.SelectedIndex = 0;
            }
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

        private void LoadCameraFormats(int deviceIndex)
        {
            comboBoxVideoFormats.Items.Clear(); // limpia antes de cargar

            if (CameraInterop.GetSupportedFormats(deviceIndex, out IntPtr ptr, out int count))
            {
                int size = Marshal.SizeOf<CameraFormat>();
                var uniqueFormats = new HashSet<string>();

                for (int i = 0; i < count; i++)
                {
                    IntPtr itemPtr = IntPtr.Add(ptr, i * size);
                    CameraFormat format = Marshal.PtrToStructure<CameraFormat>(itemPtr);

                    // clave única para evitar duplicados
                    string key = $"{format.Width}x{format.Height}@{format.FpsNumerator}/{format.FpsDenominator}";

                    if (uniqueFormats.Add(key))
                    {
                        string description = $"{format.Width}x{format.Height} @ {(format.FpsNumerator / (double)format.FpsDenominator):F2} fps";
                        comboBoxVideoFormats.Items.Add(description);
                    }
                }

                CameraInterop.FreeFormats(ptr); // liberar memoria desde C++
            }
        }


        private void btnPreview_Click(object sender, EventArgs e)
        {
            if (comboBoxCameras.SelectedItem is CameraItem selected1 && comboBoxCameras2.SelectedItem is CameraItem selected2)
            {
                IntPtr hwnd1 = pictureBoxPreview.Handle;
                IntPtr hwnd2 = pictureBoxPreview2.Handle;

                CameraInterop.StartPreview(selected1.Index, hwnd1, ref session1);
                CameraInterop.StartPreview(selected2.Index, hwnd2, ref session2);
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
                    CameraInterop.StartRecording(camIndex, micIndex, saveDialog.FileName);
                    
                }
            }
        }

        private void button2_Click(object sender, EventArgs e)
        {
            CameraInterop.PauseRecording();
        }

        private void button3_Click(object sender, EventArgs e)
        {
            if (comboBoxCameras.SelectedItem is CameraItem selected)
            {
                LoadCameraFormats(selected.Index);
            }
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

    [DllImport("CameraCaptureLib.dll")]
    public static extern bool PauseRecording();

    [DllImport("CameraCaptureLib.dll")]
    public static extern bool ResumeRecording();

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
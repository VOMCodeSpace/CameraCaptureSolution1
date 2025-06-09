namespace CameraCaptureUI
{
    partial class Form1
    {
        /// <summary>
        /// Variable del diseñador necesaria.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Limpiar los recursos que se estén usando.
        /// </summary>
        /// <param name="disposing">true si los recursos administrados se deben desechar; false en caso contrario.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Código generado por el Diseñador de Windows Forms

        /// <summary>
        /// Método necesario para admitir el Diseñador. No se puede modificar
        /// el contenido de este método con el editor de código.
        /// </summary>
        private void InitializeComponent()
        {
            this.comboBoxCameras = new System.Windows.Forms.ComboBox();
            this.btnPreview = new System.Windows.Forms.Button();
            this.pictureBoxPreview = new System.Windows.Forms.PictureBox();
            this.btnStart = new System.Windows.Forms.Button();
            this.btnPause = new System.Windows.Forms.Button();
            this.btnContinue = new System.Windows.Forms.Button();
            this.btnStop = new System.Windows.Forms.Button();
            this.comboBoxMicrophones = new System.Windows.Forms.ComboBox();
            this.comboBoxCameras2 = new System.Windows.Forms.ComboBox();
            this.pictureBoxPreview2 = new System.Windows.Forms.PictureBox();
            this.comboBoxVideoFormats = new System.Windows.Forms.ComboBox();
            this.checkBoxCamera2 = new System.Windows.Forms.CheckBox();
            this.btnReloadCameras = new System.Windows.Forms.Button();
            this.btnLoadFormats = new System.Windows.Forms.Button();
            this.label1 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.label3 = new System.Windows.Forms.Label();
            this.label4 = new System.Windows.Forms.Label();
            this.labelCam1 = new System.Windows.Forms.Label();
            this.labelCam2 = new System.Windows.Forms.Label();
            this.takePictureCam1 = new System.Windows.Forms.Button();
            this.takePictureCam2 = new System.Windows.Forms.Button();
            this.labelStatusCam1 = new System.Windows.Forms.Label();
            this.labelStatusCam2 = new System.Windows.Forms.Label();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBoxPreview)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBoxPreview2)).BeginInit();
            this.SuspendLayout();
            // 
            // comboBoxCameras
            // 
            this.comboBoxCameras.AccessibleDescription = "";
            this.comboBoxCameras.AccessibleName = "";
            this.comboBoxCameras.FormattingEnabled = true;
            this.comboBoxCameras.Location = new System.Drawing.Point(12, 48);
            this.comboBoxCameras.Name = "comboBoxCameras";
            this.comboBoxCameras.Size = new System.Drawing.Size(264, 21);
            this.comboBoxCameras.TabIndex = 0;
            // 
            // btnPreview
            // 
            this.btnPreview.Location = new System.Drawing.Point(693, 72);
            this.btnPreview.Name = "btnPreview";
            this.btnPreview.Size = new System.Drawing.Size(75, 23);
            this.btnPreview.TabIndex = 1;
            this.btnPreview.Text = "preview";
            this.btnPreview.UseVisualStyleBackColor = true;
            this.btnPreview.Click += new System.EventHandler(this.btnPreview_Click);
            // 
            // pictureBoxPreview
            // 
            this.pictureBoxPreview.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
            this.pictureBoxPreview.Location = new System.Drawing.Point(15, 157);
            this.pictureBoxPreview.Name = "pictureBoxPreview";
            this.pictureBoxPreview.Size = new System.Drawing.Size(264, 209);
            this.pictureBoxPreview.TabIndex = 2;
            this.pictureBoxPreview.TabStop = false;
            // 
            // btnStart
            // 
            this.btnStart.Location = new System.Drawing.Point(693, 117);
            this.btnStart.Name = "btnStart";
            this.btnStart.Size = new System.Drawing.Size(75, 23);
            this.btnStart.TabIndex = 3;
            this.btnStart.Text = "Start";
            this.btnStart.UseVisualStyleBackColor = true;
            this.btnStart.Click += new System.EventHandler(this.startRecording_Click);
            // 
            // btnPause
            // 
            this.btnPause.Location = new System.Drawing.Point(693, 157);
            this.btnPause.Name = "btnPause";
            this.btnPause.Size = new System.Drawing.Size(75, 23);
            this.btnPause.TabIndex = 4;
            this.btnPause.Text = "Pause";
            this.btnPause.UseVisualStyleBackColor = true;
            this.btnPause.Click += new System.EventHandler(this.pauseRecording_Click);
            // 
            // btnContinue
            // 
            this.btnContinue.Location = new System.Drawing.Point(693, 202);
            this.btnContinue.Name = "btnContinue";
            this.btnContinue.Size = new System.Drawing.Size(75, 23);
            this.btnContinue.TabIndex = 5;
            this.btnContinue.Text = "Continue";
            this.btnContinue.UseVisualStyleBackColor = true;
            this.btnContinue.Click += new System.EventHandler(this.continueRecording_Click);
            // 
            // btnStop
            // 
            this.btnStop.Location = new System.Drawing.Point(693, 246);
            this.btnStop.Name = "btnStop";
            this.btnStop.Size = new System.Drawing.Size(75, 23);
            this.btnStop.TabIndex = 6;
            this.btnStop.Text = "Stop";
            this.btnStop.UseVisualStyleBackColor = true;
            this.btnStop.Click += new System.EventHandler(this.stopRecording_Click);
            // 
            // comboBoxMicrophones
            // 
            this.comboBoxMicrophones.FormattingEnabled = true;
            this.comboBoxMicrophones.Location = new System.Drawing.Point(15, 109);
            this.comboBoxMicrophones.Name = "comboBoxMicrophones";
            this.comboBoxMicrophones.Size = new System.Drawing.Size(237, 21);
            this.comboBoxMicrophones.TabIndex = 7;
            // 
            // comboBoxCameras2
            // 
            this.comboBoxCameras2.FormattingEnabled = true;
            this.comboBoxCameras2.Location = new System.Drawing.Point(322, 48);
            this.comboBoxCameras2.Name = "comboBoxCameras2";
            this.comboBoxCameras2.Size = new System.Drawing.Size(269, 21);
            this.comboBoxCameras2.TabIndex = 8;
            // 
            // pictureBoxPreview2
            // 
            this.pictureBoxPreview2.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.pictureBoxPreview2.Location = new System.Drawing.Point(322, 157);
            this.pictureBoxPreview2.Name = "pictureBoxPreview2";
            this.pictureBoxPreview2.Size = new System.Drawing.Size(269, 209);
            this.pictureBoxPreview2.TabIndex = 9;
            this.pictureBoxPreview2.TabStop = false;
            // 
            // comboBoxVideoFormats
            // 
            this.comboBoxVideoFormats.FormattingEnabled = true;
            this.comboBoxVideoFormats.Location = new System.Drawing.Point(322, 109);
            this.comboBoxVideoFormats.Name = "comboBoxVideoFormats";
            this.comboBoxVideoFormats.Size = new System.Drawing.Size(269, 21);
            this.comboBoxVideoFormats.TabIndex = 10;
            // 
            // checkBoxCamera2
            // 
            this.checkBoxCamera2.AutoSize = true;
            this.checkBoxCamera2.Location = new System.Drawing.Point(445, 28);
            this.checkBoxCamera2.Name = "checkBoxCamera2";
            this.checkBoxCamera2.Size = new System.Drawing.Size(146, 17);
            this.checkBoxCamera2.TabIndex = 11;
            this.checkBoxCamera2.Text = "Habilitar segunda camara";
            this.checkBoxCamera2.UseVisualStyleBackColor = true;
            // 
            // btnReloadCameras
            // 
            this.btnReloadCameras.Location = new System.Drawing.Point(642, 27);
            this.btnReloadCameras.Name = "btnReloadCameras";
            this.btnReloadCameras.Size = new System.Drawing.Size(126, 23);
            this.btnReloadCameras.TabIndex = 12;
            this.btnReloadCameras.Text = "Reload Cameras";
            this.btnReloadCameras.UseVisualStyleBackColor = true;
            this.btnReloadCameras.Click += new System.EventHandler(this.reloadCameras_Click);
            // 
            // btnLoadFormats
            // 
            this.btnLoadFormats.Location = new System.Drawing.Point(471, 78);
            this.btnLoadFormats.Name = "btnLoadFormats";
            this.btnLoadFormats.Size = new System.Drawing.Size(120, 23);
            this.btnLoadFormats.TabIndex = 13;
            this.btnLoadFormats.Text = "Load Formats Cam 1";
            this.btnLoadFormats.UseVisualStyleBackColor = true;
            this.btnLoadFormats.Click += new System.EventHandler(this.loadFormats_Click);
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(12, 32);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(52, 13);
            this.label1.TabIndex = 14;
            this.label1.Text = "Camara 1";
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(12, 83);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(54, 13);
            this.label2.TabIndex = 15;
            this.label2.Text = "Microfono";
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(321, 83);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(50, 13);
            this.label3.TabIndex = 16;
            this.label3.Text = "Formatos";
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Location = new System.Drawing.Point(319, 28);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(52, 13);
            this.label4.TabIndex = 17;
            this.label4.Text = "Camara 2";
            // 
            // labelCam1
            // 
            this.labelCam1.AutoSize = true;
            this.labelCam1.Font = new System.Drawing.Font("Segoe Print", 11.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.labelCam1.Location = new System.Drawing.Point(12, 372);
            this.labelCam1.Name = "labelCam1";
            this.labelCam1.Size = new System.Drawing.Size(89, 26);
            this.labelCam1.TabIndex = 18;
            this.labelCam1.Text = "Camara 1";
            // 
            // labelCam2
            // 
            this.labelCam2.AutoSize = true;
            this.labelCam2.Font = new System.Drawing.Font("Segoe Print", 11.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.labelCam2.Location = new System.Drawing.Point(318, 369);
            this.labelCam2.Name = "labelCam2";
            this.labelCam2.Size = new System.Drawing.Size(89, 26);
            this.labelCam2.TabIndex = 19;
            this.labelCam2.Text = "Camara 2";
            // 
            // takePictureCam1
            // 
            this.takePictureCam1.Location = new System.Drawing.Point(204, 372);
            this.takePictureCam1.Name = "takePictureCam1";
            this.takePictureCam1.Size = new System.Drawing.Size(75, 23);
            this.takePictureCam1.TabIndex = 20;
            this.takePictureCam1.Text = "Tomar Foto";
            this.takePictureCam1.UseVisualStyleBackColor = true;
            // 
            // takePictureCam2
            // 
            this.takePictureCam2.Location = new System.Drawing.Point(516, 372);
            this.takePictureCam2.Name = "takePictureCam2";
            this.takePictureCam2.Size = new System.Drawing.Size(75, 23);
            this.takePictureCam2.TabIndex = 21;
            this.takePictureCam2.Text = "button2";
            this.takePictureCam2.UseVisualStyleBackColor = true;
            // 
            // labelStatusCam1
            // 
            this.labelStatusCam1.AutoSize = true;
            this.labelStatusCam1.Location = new System.Drawing.Point(102, 408);
            this.labelStatusCam1.Name = "labelStatusCam1";
            this.labelStatusCam1.Size = new System.Drawing.Size(0, 13);
            this.labelStatusCam1.TabIndex = 22;
            // 
            // labelStatusCam2
            // 
            this.labelStatusCam2.AutoSize = true;
            this.labelStatusCam2.Location = new System.Drawing.Point(442, 408);
            this.labelStatusCam2.Name = "labelStatusCam2";
            this.labelStatusCam2.Size = new System.Drawing.Size(0, 13);
            this.labelStatusCam2.TabIndex = 23;
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(800, 450);
            this.Controls.Add(this.labelStatusCam2);
            this.Controls.Add(this.labelStatusCam1);
            this.Controls.Add(this.takePictureCam2);
            this.Controls.Add(this.takePictureCam1);
            this.Controls.Add(this.labelCam2);
            this.Controls.Add(this.labelCam1);
            this.Controls.Add(this.label4);
            this.Controls.Add(this.label3);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.label1);
            this.Controls.Add(this.btnLoadFormats);
            this.Controls.Add(this.btnReloadCameras);
            this.Controls.Add(this.checkBoxCamera2);
            this.Controls.Add(this.comboBoxVideoFormats);
            this.Controls.Add(this.pictureBoxPreview2);
            this.Controls.Add(this.comboBoxCameras2);
            this.Controls.Add(this.comboBoxMicrophones);
            this.Controls.Add(this.btnStop);
            this.Controls.Add(this.btnContinue);
            this.Controls.Add(this.btnPause);
            this.Controls.Add(this.btnStart);
            this.Controls.Add(this.pictureBoxPreview);
            this.Controls.Add(this.btnPreview);
            this.Controls.Add(this.comboBoxCameras);
            this.Name = "Form1";
            this.Text = "Form1";
            this.Load += new System.EventHandler(this.Form1_Load);
            ((System.ComponentModel.ISupportInitialize)(this.pictureBoxPreview)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBoxPreview2)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.ComboBox comboBoxCameras;
        private System.Windows.Forms.Button btnPreview;
        private System.Windows.Forms.PictureBox pictureBoxPreview;
        private System.Windows.Forms.Button btnStart;
        private System.Windows.Forms.Button btnPause;
        private System.Windows.Forms.Button btnContinue;
        private System.Windows.Forms.Button btnStop;
        private System.Windows.Forms.ComboBox comboBoxMicrophones;
        private System.Windows.Forms.ComboBox comboBoxCameras2;
        private System.Windows.Forms.PictureBox pictureBoxPreview2;
        private System.Windows.Forms.ComboBox comboBoxVideoFormats;
        private System.Windows.Forms.CheckBox checkBoxCamera2;
        private System.Windows.Forms.Button btnReloadCameras;
        private System.Windows.Forms.Button btnLoadFormats;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.Label labelCam1;
        private System.Windows.Forms.Label labelCam2;
        private System.Windows.Forms.Button takePictureCam1;
        private System.Windows.Forms.Button takePictureCam2;
        private System.Windows.Forms.Label labelStatusCam1;
        private System.Windows.Forms.Label labelStatusCam2;
    }
}


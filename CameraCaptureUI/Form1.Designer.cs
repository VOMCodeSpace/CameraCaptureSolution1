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
            this.comboBoxCameras1 = new System.Windows.Forms.ComboBox();
            this.btnPreview = new System.Windows.Forms.Button();
            this.pictureBoxPreview = new System.Windows.Forms.PictureBox();
            this.btnStart = new System.Windows.Forms.Button();
            this.btnPause = new System.Windows.Forms.Button();
            this.btnContinue = new System.Windows.Forms.Button();
            this.btnStop = new System.Windows.Forms.Button();
            this.comboBoxMicrophones = new System.Windows.Forms.ComboBox();
            this.comboBoxCameras2 = new System.Windows.Forms.ComboBox();
            this.pictureBoxPreview2 = new System.Windows.Forms.PictureBox();
            this.comboBoxVideoFormatsCam1 = new System.Windows.Forms.ComboBox();
            this.checkBoxCamera2 = new System.Windows.Forms.CheckBox();
            this.btnReloadCameras = new System.Windows.Forms.Button();
            this.btnLoadFormatsCam1 = new System.Windows.Forms.Button();
            this.label1 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.label3 = new System.Windows.Forms.Label();
            this.label4 = new System.Windows.Forms.Label();
            this.labelCam1 = new System.Windows.Forms.Label();
            this.labelCam2 = new System.Windows.Forms.Label();
            this.labelStatusCam1 = new System.Windows.Forms.Label();
            this.labelStatusCam2 = new System.Windows.Forms.Label();
            this.btnTakePhotoCam1 = new System.Windows.Forms.Button();
            this.comboBoxBitrates = new System.Windows.Forms.ComboBox();
            this.BitRate = new System.Windows.Forms.Label();
            this.comboBoxVideoFormatsCam2 = new System.Windows.Forms.ComboBox();
            this.btnloadFormatsCam2 = new System.Windows.Forms.Button();
            this.label5 = new System.Windows.Forms.Label();
            this.btnTakePhotoCam2 = new System.Windows.Forms.Button();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBoxPreview)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBoxPreview2)).BeginInit();
            this.SuspendLayout();
            // 
            // comboBoxCameras1
            // 
            this.comboBoxCameras1.AccessibleDescription = "";
            this.comboBoxCameras1.AccessibleName = "";
            this.comboBoxCameras1.FormattingEnabled = true;
            this.comboBoxCameras1.Location = new System.Drawing.Point(12, 48);
            this.comboBoxCameras1.Name = "comboBoxCameras1";
            this.comboBoxCameras1.Size = new System.Drawing.Size(264, 21);
            this.comboBoxCameras1.TabIndex = 0;
            // 
            // btnPreview
            // 
            this.btnPreview.Location = new System.Drawing.Point(667, 73);
            this.btnPreview.Name = "btnPreview";
            this.btnPreview.Size = new System.Drawing.Size(126, 23);
            this.btnPreview.TabIndex = 1;
            this.btnPreview.Text = "Vista Previa";
            this.btnPreview.UseVisualStyleBackColor = true;
            this.btnPreview.Click += new System.EventHandler(this.btnPreview_Click);
            // 
            // pictureBoxPreview
            // 
            this.pictureBoxPreview.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.pictureBoxPreview.Location = new System.Drawing.Point(15, 203);
            this.pictureBoxPreview.Name = "pictureBoxPreview";
            this.pictureBoxPreview.Size = new System.Drawing.Size(264, 209);
            this.pictureBoxPreview.TabIndex = 2;
            this.pictureBoxPreview.TabStop = false;
            // 
            // btnStart
            // 
            this.btnStart.Location = new System.Drawing.Point(667, 118);
            this.btnStart.Name = "btnStart";
            this.btnStart.Size = new System.Drawing.Size(126, 23);
            this.btnStart.TabIndex = 3;
            this.btnStart.Text = "Grabar";
            this.btnStart.UseVisualStyleBackColor = true;
            this.btnStart.Click += new System.EventHandler(this.startRecording_Click);
            // 
            // btnPause
            // 
            this.btnPause.Location = new System.Drawing.Point(667, 158);
            this.btnPause.Name = "btnPause";
            this.btnPause.Size = new System.Drawing.Size(126, 23);
            this.btnPause.TabIndex = 4;
            this.btnPause.Text = "Pausar";
            this.btnPause.UseVisualStyleBackColor = true;
            this.btnPause.Click += new System.EventHandler(this.pauseRecording_Click);
            // 
            // btnContinue
            // 
            this.btnContinue.Location = new System.Drawing.Point(667, 203);
            this.btnContinue.Name = "btnContinue";
            this.btnContinue.Size = new System.Drawing.Size(126, 23);
            this.btnContinue.TabIndex = 5;
            this.btnContinue.Text = "Continuar";
            this.btnContinue.UseVisualStyleBackColor = true;
            this.btnContinue.Click += new System.EventHandler(this.continueRecording_Click);
            // 
            // btnStop
            // 
            this.btnStop.Location = new System.Drawing.Point(667, 247);
            this.btnStop.Name = "btnStop";
            this.btnStop.Size = new System.Drawing.Size(126, 23);
            this.btnStop.TabIndex = 6;
            this.btnStop.Text = "Detener";
            this.btnStop.UseVisualStyleBackColor = true;
            this.btnStop.Click += new System.EventHandler(this.stopRecording_Click);
            // 
            // comboBoxMicrophones
            // 
            this.comboBoxMicrophones.FormattingEnabled = true;
            this.comboBoxMicrophones.Location = new System.Drawing.Point(12, 160);
            this.comboBoxMicrophones.Name = "comboBoxMicrophones";
            this.comboBoxMicrophones.Size = new System.Drawing.Size(264, 21);
            this.comboBoxMicrophones.TabIndex = 7;
            // 
            // comboBoxCameras2
            // 
            this.comboBoxCameras2.FormattingEnabled = true;
            this.comboBoxCameras2.Location = new System.Drawing.Point(322, 48);
            this.comboBoxCameras2.Name = "comboBoxCameras2";
            this.comboBoxCameras2.Size = new System.Drawing.Size(271, 21);
            this.comboBoxCameras2.TabIndex = 8;
            // 
            // pictureBoxPreview2
            // 
            this.pictureBoxPreview2.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.pictureBoxPreview2.Location = new System.Drawing.Point(324, 203);
            this.pictureBoxPreview2.Name = "pictureBoxPreview2";
            this.pictureBoxPreview2.Size = new System.Drawing.Size(269, 209);
            this.pictureBoxPreview2.TabIndex = 9;
            this.pictureBoxPreview2.TabStop = false;
            // 
            // comboBoxVideoFormatsCam1
            // 
            this.comboBoxVideoFormatsCam1.FormattingEnabled = true;
            this.comboBoxVideoFormatsCam1.Location = new System.Drawing.Point(12, 108);
            this.comboBoxVideoFormatsCam1.Name = "comboBoxVideoFormatsCam1";
            this.comboBoxVideoFormatsCam1.Size = new System.Drawing.Size(264, 21);
            this.comboBoxVideoFormatsCam1.TabIndex = 10;
            // 
            // checkBoxCamera2
            // 
            this.checkBoxCamera2.AutoSize = true;
            this.checkBoxCamera2.Location = new System.Drawing.Point(467, 24);
            this.checkBoxCamera2.Name = "checkBoxCamera2";
            this.checkBoxCamera2.Size = new System.Drawing.Size(146, 17);
            this.checkBoxCamera2.TabIndex = 11;
            this.checkBoxCamera2.Text = "Habilitar segunda camara";
            this.checkBoxCamera2.UseVisualStyleBackColor = true;
            // 
            // btnReloadCameras
            // 
            this.btnReloadCameras.Location = new System.Drawing.Point(667, 28);
            this.btnReloadCameras.Name = "btnReloadCameras";
            this.btnReloadCameras.Size = new System.Drawing.Size(126, 23);
            this.btnReloadCameras.TabIndex = 12;
            this.btnReloadCameras.Text = "Cargar Cameras";
            this.btnReloadCameras.UseVisualStyleBackColor = true;
            this.btnReloadCameras.Click += new System.EventHandler(this.reloadCameras_Click);
            // 
            // btnLoadFormatsCam1
            // 
            this.btnLoadFormatsCam1.Location = new System.Drawing.Point(154, 79);
            this.btnLoadFormatsCam1.Name = "btnLoadFormatsCam1";
            this.btnLoadFormatsCam1.Size = new System.Drawing.Size(122, 23);
            this.btnLoadFormatsCam1.TabIndex = 13;
            this.btnLoadFormatsCam1.Text = "Cargar formatos Cam 1";
            this.btnLoadFormatsCam1.UseVisualStyleBackColor = true;
            this.btnLoadFormatsCam1.Click += new System.EventHandler(this.loadFormatsCam1_Click);
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
            this.label2.Location = new System.Drawing.Point(13, 139);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(54, 13);
            this.label2.TabIndex = 15;
            this.label2.Text = "Microfono";
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(14, 84);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(82, 13);
            this.label3.TabIndex = 16;
            this.label3.Text = "Formatos cam 1";
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
            this.labelCam1.Font = new System.Drawing.Font("Microsoft Sans Serif", 11.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.labelCam1.Location = new System.Drawing.Point(13, 464);
            this.labelCam1.Name = "labelCam1";
            this.labelCam1.Size = new System.Drawing.Size(73, 18);
            this.labelCam1.TabIndex = 18;
            this.labelCam1.Text = "Camara 1";
            // 
            // labelCam2
            // 
            this.labelCam2.AutoSize = true;
            this.labelCam2.Font = new System.Drawing.Font("Microsoft Sans Serif", 11.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.labelCam2.Location = new System.Drawing.Point(321, 461);
            this.labelCam2.Name = "labelCam2";
            this.labelCam2.Size = new System.Drawing.Size(73, 18);
            this.labelCam2.TabIndex = 19;
            this.labelCam2.Text = "Camara 2";
            // 
            // labelStatusCam1
            // 
            this.labelStatusCam1.AutoSize = true;
            this.labelStatusCam1.Location = new System.Drawing.Point(132, 424);
            this.labelStatusCam1.Name = "labelStatusCam1";
            this.labelStatusCam1.Size = new System.Drawing.Size(0, 13);
            this.labelStatusCam1.TabIndex = 22;
            // 
            // labelStatusCam2
            // 
            this.labelStatusCam2.AutoSize = true;
            this.labelStatusCam2.Location = new System.Drawing.Point(464, 424);
            this.labelStatusCam2.Name = "labelStatusCam2";
            this.labelStatusCam2.Size = new System.Drawing.Size(0, 13);
            this.labelStatusCam2.TabIndex = 23;
            // 
            // btnTakePhotoCam1
            // 
            this.btnTakePhotoCam1.Location = new System.Drawing.Point(173, 461);
            this.btnTakePhotoCam1.Name = "btnTakePhotoCam1";
            this.btnTakePhotoCam1.Size = new System.Drawing.Size(106, 23);
            this.btnTakePhotoCam1.TabIndex = 30;
            this.btnTakePhotoCam1.Text = "Tomar Foto Cam 1";
            this.btnTakePhotoCam1.UseVisualStyleBackColor = true;
            this.btnTakePhotoCam1.Click += new System.EventHandler(this.btnTakePhotoCam1_Click);
            // 
            // comboBoxBitrates
            // 
            this.comboBoxBitrates.FormattingEnabled = true;
            this.comboBoxBitrates.Location = new System.Drawing.Point(325, 160);
            this.comboBoxBitrates.Name = "comboBoxBitrates";
            this.comboBoxBitrates.Size = new System.Drawing.Size(269, 21);
            this.comboBoxBitrates.TabIndex = 32;
            // 
            // BitRate
            // 
            this.BitRate.AutoSize = true;
            this.BitRate.Location = new System.Drawing.Point(321, 139);
            this.BitRate.Name = "BitRate";
            this.BitRate.Size = new System.Drawing.Size(42, 13);
            this.BitRate.TabIndex = 33;
            this.BitRate.Text = "BitRate";
            // 
            // comboBoxVideoFormatsCam2
            // 
            this.comboBoxVideoFormatsCam2.FormattingEnabled = true;
            this.comboBoxVideoFormatsCam2.Location = new System.Drawing.Point(322, 108);
            this.comboBoxVideoFormatsCam2.Name = "comboBoxVideoFormatsCam2";
            this.comboBoxVideoFormatsCam2.Size = new System.Drawing.Size(271, 21);
            this.comboBoxVideoFormatsCam2.TabIndex = 34;
            // 
            // btnloadFormatsCam2
            // 
            this.btnloadFormatsCam2.Location = new System.Drawing.Point(468, 79);
            this.btnloadFormatsCam2.Name = "btnloadFormatsCam2";
            this.btnloadFormatsCam2.Size = new System.Drawing.Size(126, 23);
            this.btnloadFormatsCam2.TabIndex = 35;
            this.btnloadFormatsCam2.Text = "Cagar formatos Cam 2";
            this.btnloadFormatsCam2.UseVisualStyleBackColor = true;
            this.btnloadFormatsCam2.Click += new System.EventHandler(this.btnloadFormatsCam2_Click);
            // 
            // label5
            // 
            this.label5.AutoSize = true;
            this.label5.Location = new System.Drawing.Point(319, 84);
            this.label5.Name = "label5";
            this.label5.Size = new System.Drawing.Size(82, 13);
            this.label5.TabIndex = 36;
            this.label5.Text = "Formatos cam 2";
            // 
            // btnTakePhotoCam2
            // 
            this.btnTakePhotoCam2.Location = new System.Drawing.Point(482, 461);
            this.btnTakePhotoCam2.Name = "btnTakePhotoCam2";
            this.btnTakePhotoCam2.Size = new System.Drawing.Size(111, 23);
            this.btnTakePhotoCam2.TabIndex = 37;
            this.btnTakePhotoCam2.Text = "Tomar Foto Cam 2";
            this.btnTakePhotoCam2.UseVisualStyleBackColor = true;
            this.btnTakePhotoCam2.Click += new System.EventHandler(this.btnTakePhotoCam2_Click);
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(805, 499);
            this.Controls.Add(this.btnTakePhotoCam2);
            this.Controls.Add(this.label5);
            this.Controls.Add(this.btnloadFormatsCam2);
            this.Controls.Add(this.comboBoxVideoFormatsCam2);
            this.Controls.Add(this.BitRate);
            this.Controls.Add(this.comboBoxBitrates);
            this.Controls.Add(this.btnTakePhotoCam1);
            this.Controls.Add(this.labelStatusCam2);
            this.Controls.Add(this.labelStatusCam1);
            this.Controls.Add(this.labelCam2);
            this.Controls.Add(this.labelCam1);
            this.Controls.Add(this.label4);
            this.Controls.Add(this.label3);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.label1);
            this.Controls.Add(this.btnLoadFormatsCam1);
            this.Controls.Add(this.btnReloadCameras);
            this.Controls.Add(this.checkBoxCamera2);
            this.Controls.Add(this.comboBoxVideoFormatsCam1);
            this.Controls.Add(this.pictureBoxPreview2);
            this.Controls.Add(this.comboBoxCameras2);
            this.Controls.Add(this.comboBoxMicrophones);
            this.Controls.Add(this.btnStop);
            this.Controls.Add(this.btnContinue);
            this.Controls.Add(this.btnPause);
            this.Controls.Add(this.btnStart);
            this.Controls.Add(this.pictureBoxPreview);
            this.Controls.Add(this.btnPreview);
            this.Controls.Add(this.comboBoxCameras1);
            this.Name = "Form1";
            this.Text = "Form1";
            this.Load += new System.EventHandler(this.Form1_Load);
            ((System.ComponentModel.ISupportInitialize)(this.pictureBoxPreview)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBoxPreview2)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.ComboBox comboBoxCameras1;
        private System.Windows.Forms.Button btnPreview;
        private System.Windows.Forms.PictureBox pictureBoxPreview;
        private System.Windows.Forms.Button btnStart;
        private System.Windows.Forms.Button btnPause;
        private System.Windows.Forms.Button btnContinue;
        private System.Windows.Forms.Button btnStop;
        private System.Windows.Forms.ComboBox comboBoxMicrophones;
        private System.Windows.Forms.ComboBox comboBoxCameras2;
        private System.Windows.Forms.PictureBox pictureBoxPreview2;
        private System.Windows.Forms.ComboBox comboBoxVideoFormatsCam1;
        private System.Windows.Forms.CheckBox checkBoxCamera2;
        private System.Windows.Forms.Button btnReloadCameras;
        private System.Windows.Forms.Button btnLoadFormatsCam1;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.Label labelCam1;
        private System.Windows.Forms.Label labelCam2;
        private System.Windows.Forms.Label labelStatusCam1;
        private System.Windows.Forms.Label labelStatusCam2;
        private System.Windows.Forms.Button btnTakePhotoCam1;
        private System.Windows.Forms.ComboBox comboBoxBitrates;
        private System.Windows.Forms.Label BitRate;
        private System.Windows.Forms.ComboBox comboBoxVideoFormatsCam2;
        private System.Windows.Forms.Button btnloadFormatsCam2;
        private System.Windows.Forms.Label label5;
        private System.Windows.Forms.Button btnTakePhotoCam2;
    }
}


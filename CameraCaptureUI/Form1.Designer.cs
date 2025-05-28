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
            this.button1 = new System.Windows.Forms.Button();
            this.button2 = new System.Windows.Forms.Button();
            this.button3 = new System.Windows.Forms.Button();
            this.button4 = new System.Windows.Forms.Button();
            this.comboBoxMicrophones = new System.Windows.Forms.ComboBox();
            this.comboBoxCameras2 = new System.Windows.Forms.ComboBox();
            this.pictureBoxPreview2 = new System.Windows.Forms.PictureBox();
            this.comboBoxVideoFormats = new System.Windows.Forms.ComboBox();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBoxPreview)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBoxPreview2)).BeginInit();
            this.SuspendLayout();
            // 
            // comboBoxCameras
            // 
            this.comboBoxCameras.FormattingEnabled = true;
            this.comboBoxCameras.Location = new System.Drawing.Point(12, 60);
            this.comboBoxCameras.Name = "comboBoxCameras";
            this.comboBoxCameras.Size = new System.Drawing.Size(264, 21);
            this.comboBoxCameras.TabIndex = 0;
            // 
            // btnPreview
            // 
            this.btnPreview.Location = new System.Drawing.Point(665, 60);
            this.btnPreview.Name = "btnPreview";
            this.btnPreview.Size = new System.Drawing.Size(75, 23);
            this.btnPreview.TabIndex = 1;
            this.btnPreview.Text = "preview";
            this.btnPreview.UseVisualStyleBackColor = true;
            this.btnPreview.Click += new System.EventHandler(this.btnPreview_Click);
            // 
            // pictureBoxPreview
            // 
            this.pictureBoxPreview.Location = new System.Drawing.Point(12, 136);
            this.pictureBoxPreview.Name = "pictureBoxPreview";
            this.pictureBoxPreview.Size = new System.Drawing.Size(264, 209);
            this.pictureBoxPreview.TabIndex = 2;
            this.pictureBoxPreview.TabStop = false;
            // 
            // button1
            // 
            this.button1.Location = new System.Drawing.Point(665, 136);
            this.button1.Name = "button1";
            this.button1.Size = new System.Drawing.Size(75, 23);
            this.button1.TabIndex = 3;
            this.button1.Text = "start";
            this.button1.UseVisualStyleBackColor = true;
            this.button1.Click += new System.EventHandler(this.button1_Click);
            // 
            // button2
            // 
            this.button2.Location = new System.Drawing.Point(665, 178);
            this.button2.Name = "button2";
            this.button2.Size = new System.Drawing.Size(75, 23);
            this.button2.TabIndex = 4;
            this.button2.Text = "pause";
            this.button2.UseVisualStyleBackColor = true;
            this.button2.Click += new System.EventHandler(this.button2_Click);
            // 
            // button3
            // 
            this.button3.Location = new System.Drawing.Point(665, 207);
            this.button3.Name = "button3";
            this.button3.Size = new System.Drawing.Size(75, 23);
            this.button3.TabIndex = 5;
            this.button3.Text = "resume";
            this.button3.UseVisualStyleBackColor = true;
            this.button3.Click += new System.EventHandler(this.button3_Click);
            // 
            // button4
            // 
            this.button4.Location = new System.Drawing.Point(665, 245);
            this.button4.Name = "button4";
            this.button4.Size = new System.Drawing.Size(75, 23);
            this.button4.TabIndex = 6;
            this.button4.Text = "stop";
            this.button4.UseVisualStyleBackColor = true;
            this.button4.Click += new System.EventHandler(this.button4_Click);
            // 
            // comboBoxMicrophones
            // 
            this.comboBoxMicrophones.FormattingEnabled = true;
            this.comboBoxMicrophones.Location = new System.Drawing.Point(12, 99);
            this.comboBoxMicrophones.Name = "comboBoxMicrophones";
            this.comboBoxMicrophones.Size = new System.Drawing.Size(237, 21);
            this.comboBoxMicrophones.TabIndex = 7;
            // 
            // comboBoxCameras2
            // 
            this.comboBoxCameras2.FormattingEnabled = true;
            this.comboBoxCameras2.Location = new System.Drawing.Point(322, 60);
            this.comboBoxCameras2.Name = "comboBoxCameras2";
            this.comboBoxCameras2.Size = new System.Drawing.Size(269, 21);
            this.comboBoxCameras2.TabIndex = 8;
            // 
            // pictureBoxPreview2
            // 
            this.pictureBoxPreview2.Location = new System.Drawing.Point(322, 136);
            this.pictureBoxPreview2.Name = "pictureBoxPreview2";
            this.pictureBoxPreview2.Size = new System.Drawing.Size(269, 209);
            this.pictureBoxPreview2.TabIndex = 9;
            this.pictureBoxPreview2.TabStop = false;
            // 
            // comboBoxVideoFormats
            // 
            this.comboBoxVideoFormats.FormattingEnabled = true;
            this.comboBoxVideoFormats.Location = new System.Drawing.Point(322, 99);
            this.comboBoxVideoFormats.Name = "comboBoxVideoFormats";
            this.comboBoxVideoFormats.Size = new System.Drawing.Size(230, 21);
            this.comboBoxVideoFormats.TabIndex = 10;
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(800, 450);
            this.Controls.Add(this.comboBoxVideoFormats);
            this.Controls.Add(this.pictureBoxPreview2);
            this.Controls.Add(this.comboBoxCameras2);
            this.Controls.Add(this.comboBoxMicrophones);
            this.Controls.Add(this.button4);
            this.Controls.Add(this.button3);
            this.Controls.Add(this.button2);
            this.Controls.Add(this.button1);
            this.Controls.Add(this.pictureBoxPreview);
            this.Controls.Add(this.btnPreview);
            this.Controls.Add(this.comboBoxCameras);
            this.Name = "Form1";
            this.Text = "Form1";
            this.Load += new System.EventHandler(this.Form1_Load);
            ((System.ComponentModel.ISupportInitialize)(this.pictureBoxPreview)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBoxPreview2)).EndInit();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.ComboBox comboBoxCameras;
        private System.Windows.Forms.Button btnPreview;
        private System.Windows.Forms.PictureBox pictureBoxPreview;
        private System.Windows.Forms.Button button1;
        private System.Windows.Forms.Button button2;
        private System.Windows.Forms.Button button3;
        private System.Windows.Forms.Button button4;
        private System.Windows.Forms.ComboBox comboBoxMicrophones;
        private System.Windows.Forms.ComboBox comboBoxCameras2;
        private System.Windows.Forms.PictureBox pictureBoxPreview2;
        private System.Windows.Forms.ComboBox comboBoxVideoFormats;
    }
}


# Camera_code

**<p> This file contains code for camera. It's a backup for code that's been tested and works properly (mostly). Do NOT modify unless you are sure what you are doing. </p>**

MainWindow.xaml.cs - Responsible for camera logic
```cs
using AForge.Video;
using AForge.Video.DirectShow;
using System;
using System.Drawing;
using System.IO;
using System.Windows;
using System.Windows.Media.Imaging;

namespace Project_Camera
{
    public partial class MainWindow : Window
    {
        private Bitmap previousFrame = null;
        private FilterInfoCollection videoDevices;
        private VideoCaptureDevice videoSource;

        public MainWindow()
        {
            InitializeComponent();

            videoDevices = new FilterInfoCollection(FilterCategory.VideoInputDevice);

            MessageBox.Show($"Devices: {videoDevices.Count}");

            foreach (FilterInfo device in videoDevices)
            {
                Console.WriteLine(device.Name);
            }

            if (videoDevices.Count > 0)
            {
                videoSource = new VideoCaptureDevice(videoDevices[1].MonikerString);

                if (videoSource.VideoCapabilities.Length > 0)
                {
                    videoSource.VideoResolution = videoSource.VideoCapabilities[0];
                }

                videoSource.NewFrame += VideoSource_NewFrame;

                videoSource.Start();
                MessageBox.Show("Camera started?");

                MessageBox.Show(videoSource.IsRunning.ToString());
            }
        }

        private void VideoSource_NewFrame(object sender, NewFrameEventArgs eventArgs)
        {
            try
            {
                Bitmap currentFrame = (Bitmap)eventArgs.Frame.Clone();

                if (previousFrame != null)
                {
                    // Detect motion and draw bounding box
                    Rectangle motionRect = DetectMotion(previousFrame, currentFrame);

                    if (motionRect != Rectangle.Empty)
                    {
                        using (Graphics g = Graphics.FromImage(currentFrame))
                        {
                            g.DrawRectangle(Pens.Red, motionRect);
                        }
                    }
                }

                // Store current frame for next comparison
                previousFrame?.Dispose();
                previousFrame = (Bitmap)currentFrame.Clone();

                // Convert to WPF image
                using (MemoryStream ms = new MemoryStream())
                {
                    currentFrame.Save(ms, System.Drawing.Imaging.ImageFormat.Bmp);
                    byte[] imageBytes = ms.ToArray();

                    Dispatcher.BeginInvoke(new Action(() =>
                    {
                        using (MemoryStream stream = new MemoryStream(imageBytes))
                        {
                            BitmapImage image = new BitmapImage();
                            image.BeginInit();
                            image.StreamSource = stream;
                            image.CacheOption = BitmapCacheOption.OnLoad;
                            image.EndInit();
                            image.Freeze();

                            CameraImage.Source = image;
                        }
                    }));
                }

                currentFrame.Dispose();
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.Message);
            }
        }

        private Rectangle DetectMotion(Bitmap prev, Bitmap curr)
        {
            int width = curr.Width;
            int height = curr.Height;

            int minX = width, minY = height, maxX = 0, maxY = 0;
            int motionPixelCount = 0;

            int threshold = 100;          // higher = less sensitive
            int minMotionPixels = 500;   // ignore small noise

            for (int y = 0; y < height; y += 4)
            {
                for (int x = 0; x < width; x += 4)
                {
                    Color c1 = prev.GetPixel(x, y);
                    Color c2 = curr.GetPixel(x, y);

                    int diff = Math.Abs(c1.R - c2.R) +
                               Math.Abs(c1.G - c2.G) +
                               Math.Abs(c1.B - c2.B);

                    if (diff > threshold)
                    {
                        motionPixelCount++;

                        if (x < minX) minX = x;
                        if (y < minY) minY = y;
                        if (x > maxX) maxX = x;
                        if (y > maxY) maxY = y;
                    }
                }
            }

            // 🚨 Ignore noise / tiny movement
            if (motionPixelCount < minMotionPixels)
                return Rectangle.Empty;

            int boxWidth = maxX - minX;
            int boxHeight = maxY - minY;

            // 🚨 Ignore weird full-screen detection
            if (boxWidth > width * 0.9 && boxHeight > height * 0.9)
                return Rectangle.Empty;

            return new Rectangle(minX, minY, boxWidth, boxHeight);
        }

        protected override void OnClosed(EventArgs e)
        {
            if (videoSource != null && videoSource.IsRunning)
            {
                videoSource.SignalToStop();
                videoSource.WaitForStop();
            }

            base.OnClosed(e);
        }
    }
}
```

MainWindow.cs - Responsible for UI visual ratio
```cs
<Window x:Class="Project_Camera.MainWindow"
        xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        xmlns:d="http://schemas.microsoft.com/expression/blend/2008"
        xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/2006"
        xmlns:local="clr-namespace:Project_Camera"
        mc:Ignorable="d"
        Title="Camera Viewer" Height="450" Width="800">
    <Grid>
        <Image Name="CameraImage" Stretch="Uniform" />
    </Grid>
</Window>
```
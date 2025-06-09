const char *updateHTML = R"rawliteral(
 <!-- Designed and Developed by Rahul Morya https://in.linkedin.com/in/rahul-morya-456a3b233 -->
        </div>
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Firmware Update</title>
        <style>
            :root {
                --primary: #4361ee;
                --secondary: #3f37c9;
                --success: #4cc9f0;
                --light: #f8f9fa;
                --dark: #212529;
                --gray: #6c757d;
            }
            
            * {
                box-sizing: border-box;
                margin: 0;
                padding: 0;
            }
            
            body {
                font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
                line-height: 1.6;
                background-color: #f5f7fa;
                color: var(--dark);
                padding: 20px;
                max-width: 800px;
                margin: 0 auto;
            }
            
            .container {
                background: white;
                border-radius: 10px;
                box-shadow: 0 4px 20px rgba(0, 0, 0, 0.1);
                padding: 30px;
                margin-top: 40px;
            }
            
            h1 {
                color: var(--primary);
                text-align: center;
                margin-bottom: 30px;
                font-weight: 600;
            }
            
            .card {
                background: white;
                border-radius: 8px;
                padding: 25px;
                margin-bottom: 25px;
                box-shadow: 0 2px 10px rgba(0, 0, 0, 0.05);
                border: 1px solid rgba(0, 0, 0, 0.05);
            }
            
            .form-group {
                margin-bottom: 20px;
            }
            
            .file-input {
                display: none;
            }
            
            .file-label {
                display: block;
                padding: 15px;
                background: var(--light);
                border: 2px dashed var(--gray);
                border-radius: 8px;
                text-align: center;
                cursor: pointer;
                transition: all 0.3s ease;
            }
            
            .file-label:hover {
                border-color: var(--primary);
                background: rgba(67, 97, 238, 0.05);
            }
            
            .file-label i {
                font-size: 48px;
                color: var(--primary);
                margin-bottom: 10px;
                display: block;
            }
            
            .file-name {
                margin-top: 10px;
                font-size: 14px;
                color: var(--gray);
            }
            
            .btn {
                display: inline-block;
                background: var(--primary);
                color: white;
                border: none;
                padding: 12px 24px;
                border-radius: 6px;
                cursor: pointer;
                font-size: 16px;
                font-weight: 500;
                transition: all 0.3s ease;
                width: 100%;
                text-align: center;
            }
            
            .btn:hover {
                background: var(--secondary);
                transform: translateY(-2px);
                box-shadow: 0 4px 12px rgba(67, 97, 238, 0.2);
            }
            
            .btn:disabled {
                background: var(--gray);
                cursor: not-allowed;
                transform: none;
                box-shadow: none;
            }
            
            .progress-container {
                margin-top: 30px;
                background: var(--light);
                border-radius: 8px;
                height: 20px;
                overflow: hidden;
            }
            
            .progress-bar {
                height: 100%;
                background: linear-gradient(90deg, var(--primary), var(--success));
                width: 0%;
                transition: width 0.3s ease;
                position: relative;
            }
            
            .progress-text {
                position: absolute;
                right: 10px;
                top: 50%;
                transform: translateY(-50%);
                color: white;
                font-size: 12px;
                font-weight: bold;
            }
            
            .status {
                margin-top: 20px;
                text-align: center;
                font-weight: 500;
                color: var(--gray);
            }
            
            .instructions {
                margin-top: 30px;
                padding: 15px;
                background: #f8f9fa;
                border-radius: 8px;
                font-size: 14px;
            }
            
            .instructions h3 {
                margin-bottom: 10px;
                color: var(--primary);
            }
            
            .instructions ol {
                padding-left: 20px;
            }
            
            .instructions li {
                margin-bottom: 8px;
            }
            
            @media (max-width: 600px) {
                .container {
                    padding: 20px;
                }
                
                h1 {
                    font-size: 24px;
                }
            }
        </style>
        <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0-beta3/css/all.min.css">
    </head>
    <body>
        <div class="container">
            <h1><i class="fas fa-cloud-upload-alt"></i> Firmware Update</h1>
            
            <div class="card">
                <form id="updateForm" method="POST" action="/update" enctype="multipart/form-data">
                    <div class="form-group">
                        <input type="file" name="update" id="fileInput" class="file-input" accept=".bin">
                        <label for="fileInput" class="file-label" id="fileLabel">
                            <i class="fas fa-file-upload"></i>
                            <span>Choose firmware file or drag it here</span>
                            <div class="file-name" id="fileName">No file selected</div>
                        </label>
                    </div>
                    <button type="submit" class="btn" id="submitBtn" disabled>
                        <i class="fas fa-upload"></i> Upload & Update
                    </button>
                </form>
                
                <div class="progress-container" id="progressContainer" style="display: none;">
                    <div class="progress-bar" id="progressBar">
                        <span class="progress-text" id="progressText">0%</span>
                    </div>
                </div>
                
                <div class="status" id="statusMessage"></div>
            </div>
            
            <div class="instructions">
                <h3><i class="fas fa-info-circle"></i> Update Instructions</h3>
                <ol>
                    <li>Click "Choose firmware file" or drag your .bin file into the upload area</li>
                    <li>Verify you've selected the correct firmware file</li>
                    <li>Click "Upload & Update" to begin the process</li>
                    <li>Wait for the upload to complete (do not disconnect power)</li>
                    <li>The device will automatically reboot when finished</li>
                </ol>
            </div>
            <!-- Designed and Developed by Rahul Morya -->
        </div>
    
        <script>
            const fileInput = document.getElementById('fileInput');
            const fileLabel = document.getElementById('fileLabel');
            const fileName = document.getElementById('fileName');
            const submitBtn = document.getElementById('submitBtn');
            const progressContainer = document.getElementById('progressContainer');
            const progressBar = document.getElementById('progressBar');
            const progressText = document.getElementById('progressText');
            const statusMessage = document.getElementById('statusMessage');
            const updateForm = document.getElementById('updateForm');
            
            // Handle file selection
            fileInput.addEventListener('change', (e) => {
                if (fileInput.files.length > 0) {
                    const file = fileInput.files[0];
                    fileName.textContent = file.name;
                    submitBtn.disabled = false;
                    
                    // Validate file extension
                    if (!file.name.endsWith('.bin')) {
                        statusMessage.textContent = 'Warning: File should have .bin extension';
                        statusMessage.style.color = 'orange';
                    } else {
                        statusMessage.textContent = '';
                    }
                } else {
                    fileName.textContent = 'No file selected';
                    submitBtn.disabled = true;
                }
            });
            
            // Drag and drop functionality
            fileLabel.addEventListener('dragover', (e) => {
                e.preventDefault();
                fileLabel.style.borderColor = 'var(--primary)';
                fileLabel.style.backgroundColor = 'rgba(67, 97, 238, 0.1)';
            });
            
            fileLabel.addEventListener('dragleave', (e) => {
                e.preventDefault();
                fileLabel.style.borderColor = 'var(--gray)';
                fileLabel.style.backgroundColor = 'var(--light)';
            });
            
            fileLabel.addEventListener('drop', (e) => {
                e.preventDefault();
                fileLabel.style.borderColor = 'var(--gray)';
                fileLabel.style.backgroundColor = 'var(--light)';
                
                if (e.dataTransfer.files.length) {
                    fileInput.files = e.dataTransfer.files;
                    const event = new Event('change');
                    fileInput.dispatchEvent(event);
                }
            });
            
            // Form submission with progress
            updateForm.addEventListener('submit', function(e) {
                e.preventDefault();
                
                if (fileInput.files.length === 0) return;
                
                const file = fileInput.files[0];
                const formData = new FormData();
                formData.append('update', file);
                
                submitBtn.disabled = true;
                submitBtn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> Uploading...';
                progressContainer.style.display = 'block';
                statusMessage.textContent = 'Starting upload...';
                statusMessage.style.color = 'var(--dark)';
                
                const xhr = new XMLHttpRequest();
                
                xhr.upload.addEventListener('progress', function(evt) {
                    if (evt.lengthComputable) {
                        const percentComplete = Math.round((evt.loaded / evt.total) * 100);
                        progressBar.style.width = percentComplete + '%';
                        progressText.textContent = percentComplete + '%';
                        
                        if (percentComplete < 100) {
                            statusMessage.textContent = 'Uploading: ' + percentComplete + '%';
                        }
                    }
                }, false);
                
                xhr.addEventListener('load', function() {
                    if (xhr.status === 200) {
                        statusMessage.textContent = 'Update successful! Device will reboot...';
                        statusMessage.style.color = 'green';
                        setTimeout(() => {
                            window.location.href = '/';
                        }, 3000);
                    } else {
                        statusMessage.textContent = 'Error: ' + xhr.responseText;
                        statusMessage.style.color = 'red';
                        submitBtn.disabled = false;
                        submitBtn.innerHTML = '<i class="fas fa-upload"></i> Try Again';
                    }
                });
                
                xhr.addEventListener('error', function() {
                    statusMessage.textContent = 'Upload failed. Check connection.';
                    statusMessage.style.color = 'red';
                    submitBtn.disabled = false;
                    submitBtn.innerHTML = '<i class="fas fa-upload"></i> Try Again';
                });
                
                xhr.open('POST', '/update', true);
                xhr.send(formData);
            });
        </script>
    </body>
    </html>
     <!-- Designed and Developed by Rahul Morya https://in.linkedin.com/in/rahul-morya-456a3b233 -->

    )rawliteral";
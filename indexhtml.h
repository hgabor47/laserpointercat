#ifndef TEMPLATE_HTML_H
#define TEMPLATE_HTML_H
String HTMLO = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset='UTF-8'>
    <title>CAT LASER CONTROLLER</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        h1 { text-align: center; }
        form { margin-bottom: 20px; }
        textarea {
            width: 1000px;
            height: 400px;
            resize: none;
            font-family: monospace;
        }
        .svg-container {
            border: 1px solid #ccc;
            width: 400px;
            height: 400px;
            position: relative;
        }
        .controls { margin-top: 20px; }
        .controls input[type='text'] {
            width: 200px;
            padding: 5px;
            margin-right: 10px;
        }
        .controls input[type='number'] {
            width: 80px;
            padding: 5px;
            margin-right: 10px;
        }
        .controls button { padding: 5px 10px; }
        svg { width: 400px; height: 400px; cursor: crosshair; }
        polygon {
            fill: rgba(0, 150, 255, 0.3);
            stroke: #0096FF;
            stroke-width: 2;
        }
    </style>
</head>
<body>
    <h1>CAT LASER CONTROLLER</h1>
    
    <form id='uploadForm' action='/upload' method='POST'>
        <label for='ssid'>SSID:</label> 
        <input type='text' id='ssid' name='ssid' value=''><br><br>
        
        <label for='password'>Password:</label> 
        <input type='password' id='password' name='password'><br><br>
        
        <button type='submit'>OK</button>
    </form>
    
    <form id='uploadProgramForm' action='/uploadProgram' method='POST'>
        <label for='program'>CODES:</label><br>
        <textarea id='program' name='program'></textarea><br>
        <button type='submit'>Upload</button>
    </form>

    <form id='resetForm'>
        <button type='button' id='resetBtn'>Reset</button>
    </form>
    
    <div class='controls'>
        <label for='programName'>Code name:</label>
        <input type='text' id='programName'>
        <input type='number' id='programTime' placeholder='200' min='50' value='50'>
        <button id='clearBtn'>CLEAR</button>
        <button id='testPosBtn'>TESTPOS</button>
    </div>
    
    <div class='svg-container'>
        <svg id='drawingArea'>
            <!-- Poligon megjelenítése itt -->
        </svg>
    </div>
    
    <script>
        const svg = document.getElementById('drawingArea');
        const clearBtn = document.getElementById('clearBtn');
        const testPosBtn = document.getElementById('testPosBtn');
        const ssidInput = document.getElementById('ssid');
        const programTextarea = document.getElementById('program');
        const programNameInput = document.getElementById('programName');
        const programTimeInput = document.getElementById('programTime');
        const resetBtn = document.getElementById('resetBtn');
        resetBtn.addEventListener('click', function() {
            fetch('/reset', {
                method: 'POST'
            })
            .then(response => response.text()) // Feltételezve, hogy a válasz szöveg formátumú
            .then(message => {
                alert(message); // Az üzenetet alert-ben jelenítjük meg
                location.reload(); // Az oldal újratöltése
            })
            .catch(error => {
                console.error('Reset failed:', error);
                alert('An error occurred while resetting.');
                location.reload(); // Az oldal újratöltése
            });
        });      
        function handleFormSubmit(event, formId) {
            event.preventDefault(); // Alapértelmezett form submit viselkedés letiltása
            const form = document.getElementById(formId);
            const formData = new FormData(form);
            const url = form.action; // Az URL-t az action attribútumból olvassuk ki

            fetch(url, {
                method: 'POST',
                body: formData
            })
            .then(response => response.text()) // Feltételezve, hogy szöveg formátumú válasz érkezik
            .then(message => {
                alert(message); // Az üzenetet megjelenítjük alert-ben
                location.reload(); // Az oldal újratöltése
            })
            .catch(error => {
                console.error(`${formId} failed:`, error);
                alert('An error occurred while submitting the form.');
            });
        }

        // Upload form kezelése
        document.getElementById('uploadForm').addEventListener('submit', function(event) {
            handleFormSubmit(event, 'uploadForm');
        });

        // UploadProgram form kezelése
        document.getElementById('uploadProgramForm').addEventListener('submit', function(event) {
            handleFormSubmit(event, 'uploadProgramForm');
        });   
             
        let points = [];
        let polygonElement = null;
        let programs = { progs: [] };

        svg.addEventListener('click', function(event) {
            const rect = svg.getBoundingClientRect();
            const x = Math.max(0, Math.min(event.clientX - rect.left, svg.clientWidth));
            const y = Math.max(0, Math.min(event.clientY - rect.top, svg.clientHeight));
            let t = programTimeInput.valueAsNumber;
            if (t<50) t=50;
            
            points.push({ x, y,t });
            drawPolygon();
            updateProgramJSON();
        });

        clearBtn.addEventListener('click', function() {
            points = [];
            if (polygonElement) {
                svg.removeChild(polygonElement);
                polygonElement = null;
            }
            updateProgramJSON();
        });

        testPosBtn.addEventListener('click', function() {
            fetch('/testPos', {
                method: 'POST',
                body: JSON.stringify({ message: 'TODO - implement this endpoint' }),
                headers: { 'Content-Type': 'application/json' }
            })
            .then(response => response.json())
            .then(data => console.log('TestPos response:', data))
            .catch(error => console.error('Error:', error));
        });

        function drawPolygon() {
            if (polygonElement) {
                svg.removeChild(polygonElement);
            }
            if (points.length < 2) return;
            polygonElement = document.createElementNS('http://www.w3.org/2000/svg', 'polygon');
            const pointsAttr = points.map(p => `${p.x},${p.y}`).join(' ');
            polygonElement.setAttribute('points', pointsAttr);
            svg.appendChild(polygonElement);
        }

        function updateProgramJSON() {
            const programName = programNameInput.value.trim();
            const programTime = parseInt(programTimeInput.value, 10);

            if (!programName) {
                alert('Please fill in the Code name field.');
                return;
            }

            let lines = [];
            points.forEach((point) => {
                lines.push({'cmd': 'MOVE', 'mot': 1, 'pos': Math.min(Math.round((point.x / svg.clientWidth) * 90), 90), 'tim': point.t, 'lock': false});
                lines.push({'cmd': 'MOVE', 'mot': 0, 'pos': Math.min(Math.round((point.y / svg.clientHeight) * 90), 90), 'tim': point.t, 'lock': true});
                lines.push({'cmd': 'WAIT', 'tim': point.t});
            });

            if (points.length > 0) {
                lines.push({ cmd: 'GOTO', name: programName });
            }

            let program = programs.progs.find(p => p.name === programName);
            if (!program) {
                program = { name: programName, lines: [] };
                programs.progs.push(program);
            }
            program.lines = lines;

            programTextarea.value = formatProgramJSON(programs);
        }



        function formatProgramJSON(obj) {
            return JSON.stringify(obj, null, 2).replace(/(\{[^{}]*\})/g, match => match.replace(/\n\s*/g, ' '));
        }
        function formatProgramJSON2(obj) {
            return JSON.stringify(obj, null, 2).replace(/(\{[^{}]*\})/g, match => match.replace(/\n\s*/g, ' '));
        }

        function customJSONStringify(obj, compactLevels = [], currentLevel = 0) {
            if (typeof obj !== 'object' || obj === null) {
                return JSON.stringify(obj); // Ha nem objektum vagy null, egyszerűen string-eld
            }

            if (Array.isArray(obj)) {
                // Tömbök kezelése külön
                const isCompact = compactLevels.includes(currentLevel); // Nézzük, hogy a jelenlegi szint legyen-e tömör
                const items = obj.map(item => customJSONStringify(item, compactLevels, currentLevel + 1));

                if (isCompact) {
                    // Tömör formázás tömb esetén
                    return `[ ${items.join(', ')} ]`;
                } else {
                    // Normál formázás tömb esetén
                    const nestedIndent = '  '.repeat(currentLevel + 1);
                    const indent = '  '.repeat(currentLevel);
                    return `[\n${nestedIndent}${items.join(`,\n${nestedIndent}`)}\n${indent}]`;
                }
            }

            // Objektumok kezelése
            const isCompact = compactLevels.includes(currentLevel); // Nézzük, hogy a jelenlegi szint legyen-e tömör
            const entries = Object.entries(obj);

            if (isCompact) {
                // Egy szinten tartalmazott objektumot egysoros JSON-né alakítunk
                return `{ ${entries.map(([key, value]) => 
                    `'${key}': ${customJSONStringify(value, compactLevels, currentLevel + 1)}`
                ).join(', ')} }`;
            } else {
                // Normál formázás új sorokkal és behúzásokkal
                const indent = '  '.repeat(currentLevel);
                const nestedIndent = '  '.repeat(currentLevel + 1);
                return `{\n${entries.map(([key, value]) => 
                    `${nestedIndent}'${key}': ${customJSONStringify(value, compactLevels, currentLevel + 1)}`
                ).join(',\n')}\n${indent}}`;
            }
        }



        window.onload = function() {
            fetch('/getData')
            .then(response => response.json())
            .then(data => {
                ssidInput.value = data.ssid || '';
                //programTextarea.value = (JSON.stringify(data.progs, null, 2));
                programTextarea.value = customJSONStringify(data.code, [3]);
                //programTextarea.value = formatProgramJSON(programTextarea.value);
                programs = data.progs || { progs: [] }; //JSON.parse(programTextarea.value) || { progs: [] };
                console.log('SSID and program data loaded.');
            })
            .catch(error => console.error('Error fetching /getData:', error));
        };

</script>
</body>
</html>

)rawliteral";
#endif

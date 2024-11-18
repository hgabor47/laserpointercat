def convert_html_to_arduino_compatible(file_path: str, output_file: str):
    """
    Converts HTML content in the given file to Arduino-compatible format.
    This includes replacing double quotes with single quotes and ensuring proper formatting.

    :param file_path: Path to the input HTML file.
    :param output_file: Path to save the converted file.
    """
    try:
        # Read the input HTML file
        with open(file_path, 'r', encoding='utf-8') as file:
            html_content = file.read()
        
        # Replace all double quotes with single quotes
        converted_content = html_content.replace('"', "'")
        converted_content = converted_content.replace('http://192.168.1.228',"")
        
        # Add the Arduino `R"rawliteral(...)"` wrapper
        arduino_wrapped_content = f'#ifndef TEMPLATE_HTML_H\n#define TEMPLATE_HTML_H\nString HTMLO = R"rawliteral(\n{converted_content}\n)rawliteral";\n#endif\n'
        
        # Write the converted content to the output file
        with open(output_file, 'w', encoding='utf-8') as output:
            output.write(arduino_wrapped_content)
        
        print(f"Conversion successful! Output saved to {output_file}")
    except Exception as e:
        print(f"An error occurred: {e}")


# Paths for input and output
input_file = "index.html"  # Replace with your actual file path
output_file = "indexhtml.h"

# Run the conversion
convert_html_to_arduino_compatible(input_file, output_file)

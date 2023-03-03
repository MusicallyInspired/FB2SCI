/********************************************************************
*   FB2SCI conversion utility   v1.00                               *
*   by Brandon Blume                                                *
*   shine62@gmail.com                                               *
*   February 25, 2023                                               *
*                                                                   *
*   Command line tool to convert 2 FB-01 sysex bank files into      *
*   Sierra's IMF/FB-01 patch resource format for SCI0 games.        *
*                                                                   *
*   You're free to do with it as you please. This program could     *
*   probably be vastly improved to be more efficient, but it works. *
********************************************************************/

#include <fstream>
#include <iostream>
#include <vector>
#include <iomanip>

using namespace std;

float nVersion = 1.00;

void read_files(ifstream& file1, ifstream& file2, vector<char>& data1, vector<char>& data2);
void reorganize_data(vector<char>& data1, vector<char>& data2);
void write_to_file(std::vector<char> data1, std::vector<char> data2, const char* output_filename);
bool check_file_exists(const char* filename);
void check_output_file(string output_filename);

int main(int argc, char* argv[]) {
    // Check if the user provided exactly three arguments

    std::cout << std::fixed;
    std::cout << std::setprecision(2);
    cout << "\nFB2SCI  v" << nVersion << "    by Brandon Blume    February 25, 2023" << endl;

    if (argc != 4) {
        cout << "   usage:  " << argv[0] << "   bankfile1   bankfile2   patfile\n";
        return 1;
    }
    cout << endl;

    // Get the filenames from the command line arguments
    char* input_filename1 = argv[1];
    char* input_filename2 = argv[2];
    char* output_filename = argv[3];

    // Open the first input bank file (Bank A)
    ifstream input_file1(input_filename1, ios::binary);

    // Check if bankfile1 exists
    if (!check_file_exists(input_filename1)) {
        cout << "Error: file " << input_filename1 << " not found" << endl;
        exit(EXIT_FAILURE);
    }

    // Read the first 7 bytes from the file
    char header[7];
    input_file1.read(header, 7);

    // Check if the header for bankfile1 matches the expected value for the FB-01's send Bank A sysex code
    if (memcmp(header, "\xF0\x43\x75\x00\x00\x00\x00", 7) != 0) {
        cout << "Error: " << input_filename1 << " is not a valid FB-01 sysex bank file (missing expected sysex header)." << endl;
        exit(EXIT_FAILURE);
    }

    // Get the length of the file
    input_file1.seekg(0, ios::end);
    std::streamoff length = input_file1.tellg();
    input_file1.seekg(0, ios::beg);

    // Check if the length is 6363 bytes (must be no larger or smaller
    if (length != 6363) {
        cout << input_filename1 << " is not the expected size (6363 bytes). Not a valid FB-01 sysex bank file." << endl << "Actual size: " << length << endl;
        exit(EXIT_FAILURE);
    }

    // Open the second input bank file (Bank B)
    ifstream input_file2(input_filename2, ios::binary);

    // Check if bankfile2 exists
    if (!check_file_exists(input_filename2)) {
        cout << "Error: file " << input_filename2 << " not found" << endl;
        exit(EXIT_FAILURE);
    }

    // Read the first 7 bytes from the file
    input_file2.read(header, 7);

    // Check if the header for bankfile2 matches the expected value for the FB-01's send Bank B sysex code
    if (memcmp(header, "\xF0\x43\x75\x00\x00\x00\x01", 7) != 0) {
        cout << "Error: " << input_filename2 << " is not a valid FB-01 sysex bank file (missing expected sysex header)." << endl;
        exit(EXIT_FAILURE);
    }

    // Get the length of the file
    input_file2.seekg(0, ios::end);
    length = input_file2.tellg();
    input_file2.seekg(0, ios::beg);

    // Check if the length is 6363 bytes (no larger or smaller)
    if (length != 6363) {
        cout << input_filename2 << " is not the expected size (6363 bytes). Not a valid FB-01 sysex bank file." << endl << "Actual size: " << length << endl;
        exit(EXIT_FAILURE);
    }

    // Check if output file already exists. If it does, ask user whether to overwrite or abort.
    check_output_file(output_filename);

    // Read the files into memory
    vector<char> data1, data2;
    read_files(input_file1, input_file2, data1, data2);

    // Close the input files
    input_file1.close();
    input_file2.close();


    // Byte-swap then nibble-merge the data, overwriting and truncating the vectors by half
    reorganize_data(data1, data2);
    // Create the patch file with the new "denibbled" data
    write_to_file(data1, data2, output_filename);

    cout << "SCI FB-01 Patch created successfully!" << endl;

    return 0;
}

void read_files(ifstream& file1, ifstream& file2, vector<char>& data1, vector<char>& data2) {
    // Set the initial read position for each bank file to address 0x4C (this is where the first instrument packet's patch data is located)
    streampos pos1 = 0x4c;
    streampos pos2 = 0x4C;

    // Read the files one at a time, skipping 3 bytes between each 128-byte instrument patch block
    //   (the last byte in a packet is that packet's checksum and the first two bytes of the next packet are the next packet's
    //    size identifier. we already skipped the two packet size indentifier bytes in the first packet by jumping straight to address 0x4C)
    for (int i = 0; i < 48; i++) {
        // Read 128 bytes from bankfile1 and bankfile2
        file1.seekg(pos1);
        file2.seekg(pos2);
        char buffer1[128];
        char buffer2[128];
        file1.read(buffer1, 128);
        file2.read(buffer2, 128);
        data1.insert(data1.end(), buffer1, buffer1 + 128);
        data2.insert(data2.end(), buffer2, buffer2 + 128);

        // Skip 3 bytes (the current packet's checksum and the following packet's size identifier bytes) to get to the next instrument's patch data
        pos1 += 131;
        pos2 += 131;
    }
}

void reorganize_data(vector<char>& data1, vector<char>& data2) {
    // Check if the data vectors have the same size. (really, if the input files passed the format checks, this should never error)
    if (data1.size() != data2.size()) {
        cout << "Error: data vectors have different sizes" << endl;
        cout << "data1 size = " << data1.size() << endl << "data2 size = " << data2.size() << endl;
        return;
    }
    // Double check to ensure that both sets of instrument packets equal 6144 bytes in length (128 bytes per 48 instruments per bank file).
    // Again, this should never error. Probably superfluous.
    else if (data1.size() != 6144) {
        cout << "Error: data vectors not the expected size (6144)" << endl;
        cout << "data1 size = " << data1.size() << endl << "data2 size = " << data2.size() << endl;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////
    //  Now we must byte-swap and nibble-merge each byte pair in every instrument packet.       //
    //  This will extract the raw patch data that SCI's patch format needs. This will reduce    //
    //  the packet size for each instrument from 128 bytes to 64 bytes so after the bytes       //
    //  are "denibblized", we will halve the data vectors sizes                                 //
    //////////////////////////////////////////////////////////////////////////////////////////////

    // Iterate over each byte pair of the data vectors
    for (int i = 0; i < static_cast<int>(data1.size()); i += 2) {
        // Extract the high and low bytes for the first byte pairs
        char high_byte1 = data1[i];
        char low_byte1 = data1[i + 1];
        char high_byte2 = data2[i];
        char low_byte2 = data2[i + 1];

        // Merge the byte pairs by shifting the low byte's low nibble to its high nibble and OR-ing the high byte's low nibble together 
        // with the low byte's new high nibble
        unsigned char merged_byte1 = ((low_byte1 & 0x0F) << 4) | (high_byte1 & 0x0F);
        unsigned char merged_byte2 = ((low_byte2 & 0x0F) << 4) | (high_byte2 & 0x0F);

        // Store the reorganized data back to the data vector at the beginning where it left off
        data1[i / 2] = merged_byte1;
        data1[(i / 2) + 1] = 0x00;
        data2[i / 2] = merged_byte2;
        data2[(i / 2) + 1] = 0x00;
    }

    // Halve the size of both data vectors now that the "denibblized" data is half the original size
    data1.resize(data1.size() / 2);
    data2.resize(data2.size() / 2);
}

void write_to_file(std::vector<char> data1, std::vector<char> data2, const char* output_filename) {
    // Open the output file in binary mode for writing
    std::ofstream out_file(output_filename, std::ios::binary);
    out_file.seekp(0, std::ios::beg);

    //////////////////////////////////////////////////////////////////////////////////////////
    //  The FB-01 SCI Patch file format we must create is structured like so:               //
    //                                                                                      //
    //  $00 :   8900h.......................SCI's resource type identifier header           //
    //  $02 :   Bank 1 data.................First 48 instrument patches (64 bytes each)     //
    //  $C02:   ABCDh.......................Seperator bytes between the two banks           //
    //  $C04:   Bank 2 data.................Last 48 instrument patches (64 bytes each)      //
    //                                                                                      //
    //  The resulting file will be exactly 6148 bytes long.                                 //
    //////////////////////////////////////////////////////////////////////////////////////////

    char sciPatchHeader[2] = { '\x89', '\x00' };
    out_file.write(reinterpret_cast<char*>(&sciPatchHeader), sizeof(sciPatchHeader));
    out_file.write(data1.data(), data1.size());
    char bankSeparator[2] = { '\xAB', '\xCD' };
    out_file.write(reinterpret_cast<char*>(&bankSeparator), sizeof(bankSeparator));
    out_file.write(data2.data(), data2.size());
    out_file.close();
}

bool check_file_exists(const char* filename) {
    std::ifstream infile(filename);
    return infile.good();
}

void check_output_file(string output_filename) {
    ifstream file(output_filename);
    if (file.good()) {
        cout << "Output file already exists. Do you want to overwrite it? (Y/N): ";
        string answer;
        cin >> answer;
        if (answer == "Y" || answer == "y") {
            ofstream file(output_filename, ios::trunc);
            cout << "\nFile " << output_filename << " successfully wiped.\n" << endl;
            file.close();
        }
        else {
            cout << "Aborting operation..." << endl;
            exit(EXIT_FAILURE);
        }
    }
}

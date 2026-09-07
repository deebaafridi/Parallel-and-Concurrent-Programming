#include <iostream>
#include <vector>
#include <fstream>
#include <thread>
#include <atomic>
#include <memory>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <climits>
#include <mutex>
#include <sstream>
using namespace std;

// Global variables for input and output files
string inputFileName = "../Input-Output/input_params.txt";
string outputFileName = "../Input-Output/output-wait-free.txt";

// Parameters read from the input file
vector<double> inputFileValues;
int numberOfWriterThreads;
int numberOfReaderThreads;
int sizeOfRegister;
double waitTimeForNextWrite;
double waitTimeForNextRead;
int totalNumberOfSnapshots;

// Mutexes for controlling output printing and protecting the register array
mutex printMutex;
mutex registerMutex;

// Structure to hold a register value with its associated timestamp (stamp)
struct StampedValue 
{
    atomic<int> stamp;
    int value;

    StampedValue(int init_value = 0) : value(init_value), stamp(0) {}
    StampedValue(int stamp_value, int init_value) : value(init_value), stamp(stamp_value) {}
    StampedValue(const StampedValue& other) : value(other.value), stamp(other.stamp.load()) {}
};

vector<shared_ptr<StampedValue>> registerArray;

// Function to read input file and store values in inputFileValues
int readingInputFile() 
{
    ifstream inputFile(inputFileName);

    if (!inputFile) 
    {
        return 1;
    }

    double value;

    while (inputFile >> value) 
    {
        inputFileValues.push_back(value);
    }

    inputFile.close();

    return 0;
}

// Function to fetch input details (e.g., number of threads, wait times) from the read input values
void fetchingInputDetails() 
{
    numberOfWriterThreads = static_cast<int>(inputFileValues[0]);
    numberOfReaderThreads = static_cast<int>(inputFileValues[1]);
    sizeOfRegister = static_cast<int>(inputFileValues[2]);
    waitTimeForNextWrite = inputFileValues[3];
    waitTimeForNextRead = inputFileValues[4];
    totalNumberOfSnapshots = static_cast<int>(inputFileValues[5]);
}

// Function to get the current system time in hours, minutes, seconds, and milliseconds
string getCurrentSystemTime() 
{
    auto now = chrono::system_clock::now();
    auto now_time_t = chrono::system_clock::to_time_t(now);
    auto now_tm = *localtime(&now_time_t);
    auto now_ms = chrono::time_point_cast<chrono::milliseconds>(now);
    auto value = now_ms.time_since_epoch();
    long milliseconds = value.count() % 1000;
    ostringstream oss;
    oss << put_time(&now_tm, "%H:%M:%S") << ":" << setw(3) << setfill('0') << milliseconds;

    return oss.str();
}

// Function to update a register at a specific index with a new value
void update(int index, int value) 
{
    int maxStamp = -1;

    lock_guard<mutex> lock(registerMutex);

    for (int i = 0; i < sizeOfRegister; ++i) 
    {
        maxStamp = max(maxStamp, registerArray[i]->stamp.load());
    }

    auto newValue = make_shared<StampedValue>(maxStamp + 1, value);

    registerArray[index] = newValue;
}
    
// Function to collect a snapshot of the register array
vector<shared_ptr<StampedValue>> collect() 
{
    vector<shared_ptr<StampedValue>> copy(sizeOfRegister);

    lock_guard<mutex> lock(registerMutex); 

    for (int j = 0; j < sizeOfRegister; j++) 
    {
        copy[j] = make_shared<StampedValue>(*registerArray[j]);
    }

    return copy;
}

// Writer thread operation: updates a register with a random value at a random location
void writerThreadOperation(atomic<bool>& term, int processId, ofstream &outputFile) 

{
    while (term.load()) 
    {
        int registerIndex = processId % sizeOfRegister;
        int value = rand() % sizeOfRegister;
        int positionOfChange = rand() % sizeOfRegister;

        update(positionOfChange, value);

        string time = getCurrentSystemTime();

        lock_guard<mutex> lock(printMutex);

        outputFile << "Thr " << processId + 1 << "'s write of " << value << " on location " << positionOfChange<<" at "<<time << endl;

        this_thread::sleep_for(chrono::microseconds(static_cast<int>(waitTimeForNextWrite)));
    }
}

// Snapshot thread operation (Wait-Free version): collects snapshots and checks for consistency
void snapshotThreadOperationWaitFree(atomic<bool>& term, int snapshotId, ofstream &outputFile)
{
    vector<shared_ptr<StampedValue>> oldCopy, newCopy;
    vector<bool> moved(sizeOfRegister, false);  
    
    bool changed = false;
    int count = 0;  

    oldCopy = collect();  

    while (term.load() && count < totalNumberOfSnapshots) 
    {
        newCopy = collect();  
        changed = false;

        // Compare old copy and new copy for changes
        for (int j = 0; j < sizeOfRegister; j++) 
        {
            if (oldCopy[j]->stamp.load() != newCopy[j]->stamp.load()) 
            {  
                if (moved[j]) 
                {  
                    // If the register has already been processed, finalize the snapshot
                    lock_guard<mutex> lock(printMutex);
                    ostringstream snapshotStream;

                    snapshotStream << "Thr" << snapshotId + 1 << "'s snapshot: ";
                    
                    for (int k = 0; k < sizeOfRegister; k++) 
                    {
                        snapshotStream << "L" << k + 1 << "-" << newCopy[k]->value;

                        if (k < sizeOfRegister - 1) 
                        {
                            snapshotStream << " ";
                        }
                    }

                    string time = getCurrentSystemTime();

                    snapshotStream << " which finished at " << time << endl;

                    outputFile << snapshotStream.str();

                    return;
                } 
                else 
                {
                    moved[j] = true;
                    oldCopy = newCopy;  
                    changed = true;

                    break;  
                }
            }
        }

        // If no registers have moved, this snapshot is consistent
        if (!changed) 
        {
            count++;

            lock_guard<mutex> lock(printMutex);
            
            ostringstream snapshotStream;

            snapshotStream << "Thr" << snapshotId + 1 << "'s snapshot: ";
            
            // Print the values of the registers in the snapshot
            for (int j = 0; j < sizeOfRegister; j++) 
            {
                snapshotStream << "L" << j + 1 << "-" << newCopy[j]->value;

                if (j < sizeOfRegister - 1) 
                {
                    snapshotStream << " ";
                }
            }
            string time = getCurrentSystemTime();

            snapshotStream << " which finished at " << time << endl;

            outputFile << snapshotStream.str();
        }

        this_thread::sleep_for(chrono::microseconds(static_cast<int>(waitTimeForNextRead)));
    }

    lock_guard<mutex> lock(printMutex);

    outputFile << "Snapshot thread " << snapshotId << " finished after collecting " << count << " snapshots." << endl;
}


int main() 
{
    srand(static_cast<unsigned int>(time(0)));
    
    // Reading input values from the file
    if (readingInputFile()) 
    {
        cout << "Unable to read file" << endl;

        return 0;
    }

    fetchingInputDetails();

    registerArray.resize(sizeOfRegister);

    for (int i = 0; i < sizeOfRegister; ++i) 
    {
        registerArray[i] = make_shared<StampedValue>(0);
    }
    
    // Opening the output file to write the log
    ofstream outputFile(outputFileName);

    if(!outputFile)
    {
        cout<<" Unable to open the output file"<<endl;

        return 1;
    }

    vector<thread> writers;
    vector<thread> readers;

    atomic<bool> term(true);

    // Launch writer threads
    for (int i = 0; i < numberOfWriterThreads; ++i) 
    {
        writers.emplace_back(writerThreadOperation, ref(term), i, ref(outputFile));
    }

    // Launch snapshot threads
    for (int i = 0; i < numberOfReaderThreads; ++i) 
    {
        readers.emplace_back(snapshotThreadOperationWaitFree, ref(term), i, ref(outputFile));
    }

    this_thread::sleep_for(chrono::seconds(10));  

    term.store(false);

    for (auto& writer : writers) 
    {
        writer.join();
    }

    for (auto& reader : readers) 
    {
        reader.join();
    }

    outputFile.close();

    return 0;
}

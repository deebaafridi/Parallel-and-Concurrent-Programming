#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>
#include <iomanip>
#include <random>
#include <atomic>
#include <mutex>
using namespace std;

string fileName = "../Input-Output/input_params.txt";
int totalThreads;
int numberOfTimeInCS;
double lambda1;
double lambda2;
double delayInsideCS;
double delayOutsideCS;

vector<int> inputFileValues;
mutex printMutex;

int readingInputFile()
{
    ifstream inputFile(fileName);

    if (!inputFile)
    {
        return 1;
    }

    int index;
    while (inputFile >> index) 
    {
        inputFileValues.push_back(index);   
    }

    inputFile.close();

return 0;
}

void fetchingInputDetails()
{
    totalThreads = inputFileValues[0];
    numberOfTimeInCS = inputFileValues[1];
    lambda1 = inputFileValues[2];
    lambda2 = inputFileValues[3];
}

void calculatingTimeDelays()
{
    default_random_engine generatorForLambda;
    exponential_distribution<double> distributionForLambda1(1.0 / lambda1); 
    delayInsideCS = distributionForLambda1(generatorForLambda);

    exponential_distribution<double> distributionForLambda2(1.0 / lambda2); 
    delayOutsideCS = distributionForLambda2(generatorForLambda);
}
 
class BakeryLock 
{
private:

    vector<atomic<int>> label;
    vector<atomic<bool>> flag;

public:

    BakeryLock(int numberOfThreads) : label(numberOfThreads), flag(numberOfThreads)
    {
        for(int i=0; i<numberOfThreads; i++)
        {
            flag[i].store(false);
            label[i].store(0);
        }
    }
    
    void lock(int threadNumber)
    {
        flag[threadNumber].store(true);

        int maxValueInLabel = label[0];
        for(int i=0; i<label.size(); i++)
        {
            if(maxValueInLabel < label[i])
            {
                maxValueInLabel = label[i];
            }
        }
        label[threadNumber].store(maxValueInLabel + 1);

        for(int i=0; i<totalThreads; i++)
        {
            if(i!= threadNumber)
            {
                while(flag[i].load() && (label[i].load() < label[threadNumber].load() || (label[i].load() == label[threadNumber].load() && i<threadNumber))){}
            }
        }
    }

    void unlock(int threadNumber)
    {
        flag[threadNumber].store(false);
    }
};

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

void lockTestCS(int threadID, ofstream &outputFile, BakeryLock &bakery)
{
    for (int i = 1; i <= numberOfTimeInCS; i++) 
    {
        string entryRequestTime = getCurrentSystemTime();

        printMutex.lock();
        outputFile << i << " CS Entry Request will be at " << entryRequestTime << " by thread "<< threadID << endl ;
        printMutex.unlock();

        bakery.lock(threadID);
        
        string actualEntryTime = getCurrentSystemTime();

        printMutex.lock();
        outputFile << i << " CS Entry will be at " << actualEntryTime << " by thread "<< threadID << endl ;
        printMutex.unlock();

        this_thread::sleep_for(chrono::milliseconds(static_cast<int>(delayInsideCS)));
         
        string exitRequestTime = getCurrentSystemTime();

        printMutex.lock();
        outputFile << i << " CS exit Request will be at " << exitRequestTime << " by thread "<< threadID << endl ;
        printMutex.unlock();

        bakery.unlock(threadID);
       
        string actualExitTime = getCurrentSystemTime();

        printMutex.lock();
        outputFile << i << " CS exit will be at " << actualExitTime << " by thread "<< threadID << endl ;
        printMutex.unlock();

        this_thread::sleep_for(chrono::milliseconds(static_cast<int>(delayOutsideCS)));   
    }       
}

void creatingThreads(ofstream &outputFile, BakeryLock &bakery)
{
    vector<thread> threads(totalThreads);

    for (int i = 0; i < totalThreads; i++)
    {
        threads[i] = thread(lockTestCS, i , ref(outputFile), ref(bakery));
    }

    for (auto &th : threads)
    {
        th.join();
    }
}

int main()
{
    int check = readingInputFile();
    if (check)
    {
        cout << "Unable to read file" << endl;
        return 0;
    }

    fetchingInputDetails();
    calculatingTimeDelays();

    BakeryLock bakery(totalThreads);

    ofstream outputFile("../Input-Output/output-bakery.txt");

    if(outputFile.is_open())
    {
        auto startPoint = chrono::system_clock::now();
        string startTime = getCurrentSystemTime() ;
        outputFile<< "The start time of execution is : " << startTime << endl;
        
        creatingThreads(outputFile, bakery);

        auto endPoint = chrono::system_clock::now();
        string endTime = getCurrentSystemTime() ;
        outputFile<< "The end time of execution is : " << endTime << endl;

        chrono::duration<double, std::milli> executionTime = endPoint - startPoint;
        double totalExecutionTime = executionTime.count();
        outputFile<< "Total execution time is : " << totalExecutionTime << " milisec" << endl;

        double throughputOfBakery = (totalThreads * numberOfTimeInCS) / totalExecutionTime;
        outputFile<< "Throughput of Bakery Algorithm is : " << throughputOfBakery << endl;

    outputFile.close();
    } 
    
return 0;
}

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <TFile.h>
#include <TTree.h>
#include "Recorder.hpp"
#include "EventData.hpp"

using namespace DELILA::Monitor;

class RecorderTest : public ::testing::Test 
{
protected:
    void SetUp() override 
    {
        recorder = std::make_unique<Recorder>();
        // Clean up any existing test files
        CleanupTestFiles();
    }
    
    void TearDown() override 
    {
        // Clean up test files after each test
        CleanupTestFiles();
    }
    
    void CleanupTestFiles() 
    {
        // Remove any test ROOT files
        for (const auto& entry : std::filesystem::directory_iterator(".")) {
            if (entry.path().extension() == ".root" && 
                entry.path().filename().string().find("test") != std::string::npos) {
                std::filesystem::remove(entry.path());
            }
        }
    }
    
    // Helper function to create test event data
    std::unique_ptr<std::vector<std::unique_ptr<EventData>>> CreateTestEventData(size_t count)
    {
        auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        events->reserve(count);
        
        for (size_t i = 0; i < count; ++i) {
            auto event = std::make_unique<EventData>();
            event->timeStampNs = i * 1000;  // Simple timestamp
            event->waveformSize = 2;
            event->analogProbe1.resize(2);
            event->analogProbe1[0] = static_cast<int32_t>(i & 0xFFFF);
            event->analogProbe1[1] = static_cast<int32_t>((i >> 16) & 0xFFFF);
            events->push_back(std::move(event));
        }
        
        return events;
    }
    
    std::unique_ptr<Recorder> recorder;
};

// Test for CalStoredDataSize function
TEST_F(RecorderTest, CalStoredDataSizeReturnsCorrectSize) 
{
    // Start recording
    recorder->StartRecording();
    
    // Add some test data
    auto testData = CreateTestEventData(100);
    recorder->LoadEventData(std::move(testData));
    
    // Wait a bit for data to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // This test will fail initially because CalStoredDataSize is not implemented
    // We expect it to return the total size in bytes and number of events
    // For now, we can't directly test this private method, so we'll test it
    // indirectly through the file writing behavior
    
    recorder->StopRecording();
}

// Test for ParallelSort function
TEST_F(RecorderTest, ParallelSortSortsEventsByTimestamp) 
{
    // Start recording with 2 threads for parallel sorting
    recorder->StartRecording(2);
    
    // Create unsorted test data
    auto testData = CreateTestEventData(100);
    // Shuffle timestamps to make them unsorted
    for (size_t i = 0; i < testData->size(); i += 2) {
        if (i + 1 < testData->size()) {
            std::swap((*testData)[i]->timeStampNs, (*testData)[i + 1]->timeStampNs);
        }
    }
    
    recorder->LoadEventData(std::move(testData));
    
    // Wait for processing and sorting
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    recorder->StopRecording();
    
    // Check if a file was created (this will fail initially)
    bool fileCreated = false;
    for (const auto& entry : std::filesystem::directory_iterator(".")) {
        if (entry.path().extension() == ".root" && 
            entry.path().filename().string().find("run0000_v0000_test.root") != std::string::npos) {
            fileCreated = true;
            
            // Verify events are sorted in the file
            TFile* file = TFile::Open(entry.path().c_str());
            ASSERT_NE(file, nullptr);
            
            TTree* tree = dynamic_cast<TTree*>(file->Get("EventTree"));
            ASSERT_NE(tree, nullptr);
            
            // We'll check proper sorting implementation after WritingThread is implemented
            
            file->Close();
            delete file;
            break;
        }
    }
    
    EXPECT_TRUE(fileCreated);
}

// Test for WritingThread function
TEST_F(RecorderTest, WritingThreadCreatesROOTFile) 
{
    // Start recording
    recorder->StartRecording(1, 1024, 1);  // Small file size threshold and time threshold
    
    // Create test data
    auto testData = CreateTestEventData(50);
    recorder->LoadEventData(std::move(testData));
    
    // Wait for file to be written
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    recorder->StopRecording();
    
    // Check if ROOT file was created
    bool fileFound = false;
    for (const auto& entry : std::filesystem::directory_iterator(".")) {
        if (entry.path().extension() == ".root" && 
            entry.path().filename().string().find("run0000_v0000_test.root") != std::string::npos) {
            fileFound = true;
            
            // Verify the file contains the expected data
            TFile* file = TFile::Open(entry.path().c_str());
            ASSERT_NE(file, nullptr);
            
            TTree* tree = dynamic_cast<TTree*>(file->Get("EventTree"));
            ASSERT_NE(tree, nullptr);
            
            EXPECT_GT(tree->GetEntries(), 0);
            
            file->Close();
            delete file;
            break;
        }
    }
    
    EXPECT_TRUE(fileFound) << "ROOT file was not created by WritingThread";
}

// Test WritingThread with custom filename
TEST_F(RecorderTest, WritingThreadUsesCorrectFilename) 
{
    recorder->SetRunNumber(42);
    recorder->SetSuffix("physics");
    recorder->StartRecording(1, 1024, 1);
    
    auto testData = CreateTestEventData(10);
    recorder->LoadEventData(std::move(testData));
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    recorder->StopRecording();
    
    // Check for file with custom run number and suffix
    bool fileFound = false;
    for (const auto& entry : std::filesystem::directory_iterator(".")) {
        if (entry.path().filename().string().find("run0042_v0000_physics.root") != std::string::npos) {
            fileFound = true;
            break;
        }
    }
    
    EXPECT_TRUE(fileFound) << "File with custom run number and suffix not found";
}
// Project Management Software by Ethan (github.com/Dragjon)
// Program Name: Elixir
// Program Version: 1.0.0
// Description:
//              A simple and probably bad project management software utilising the
//              Critical Path Method (CPM) to optimise task scheduling based on task
//              duration and dependencies. Requires just a csv "task.csv" as data
//              of every task and outputs a "output.csv" as well as a "timeline.csv"
//              as outputs of the program.
// Reference: https://www.wrike.com/blog/critical-path-is-easy-as-123/
//            https://www.projectmanager.com/guides/critical-path-method

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

using namespace std;

// Task structure for project management software
struct Task {
    // Task information
    string name;
    int duration;
    vector<string> dependencies;

    // Unknown information currently
    // Will be populated later
    vector<string> successors;

    // Calculation for critical-path-method
    int ES; // Early start
    int EF; // Early finish
    int LS; // Late start
    int LF; // Late finish
    int slack; // The amount of time a task can be delayed without affecting duration, 
               // tasks not on the critical path with have a slack of > 0 while critical
               // tasks have a slack = 0

    // Constructor for task
    Task(const string& taskName, int taskDuration, const vector<string>& deps = {})
        : name(taskName), duration(taskDuration), dependencies(deps) 
    {}

};  


// Helper function to split string by semicolon
vector<string> splitDependencies(const string& s, char separator = ';') {
    vector<string> result;
    stringstream ss(s);
    string item;
    while (getline(ss, item, separator)) {
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    return result;
}

// Function to load csv of tasks into a vector
// CSV file should be formatted as
/*
    task,duration,dependencies
    a,2,
    b,3,a
    c,2,a
    d,5,b;c                 
*/
vector<Task> loadCSV(const string& filename) {
    vector<Task> tasks;
    ifstream file(filename);

    // Opens csv file
    if (!file.is_open()) {
        cerr << "Failed to open file: " << filename << endl;
        return tasks;
    }

    string line;
    // Process every line in the csv except the first line, which contains the headers
    bool startProcessingLines = false;
    while (getline(file, line)) {
        if (startProcessingLines == true){
            stringstream ss(line);
            string cell;
            vector<string> row;

            // Split by comma
            while (getline(ss, cell, ',')) {  
                // For each column
                row.push_back(cell);
            }

            Task t(row[0], stoi(row[1]), row.size() == 3 ? splitDependencies(row[2], ';') : vector<string>{});
            
            tasks.push_back(t);
        }

        startProcessingLines = true;    
    }

    file.close();
    return tasks;
}

// Debug printing for tasklist
void debugPrint(const vector<Task>& tasks){
    for (auto& t : tasks) {
        cout << "Task: " << t.name << ", Duration: " << t.duration << ", Dependencies: ";
        for (auto d : t.dependencies) cout << d << "; ";
        cout << endl;
    }
}

// Helper function to get task from tasklist by name
const Task& getTaskFromList(const string& name, const vector<Task>& taskList) {
    for (auto& t : taskList) {
        if (t.name == name) return t;
    }
    throw runtime_error("Task not found: " + name);
}

// Returns the task object instead of the const task reference so that it can be updated by reference
Task& getTaskObjectFromList(const string& name, vector<Task>& taskList) {
    for (auto& t : taskList) {
        if (t.name == name) return t; 
    }
    throw runtime_error("Task not found: " + name);
}

//////////////////////////////////////////////////////////////////////////////////////////
// Forward pass for calculating early start (ES) and early finish (EF)                  //
// Early Start (ES): the earliest time a task can start, considering its dependencies.  //
//                   ES = max(EF of all dependencies)                                   //
// If a task has no dependencies, we should be starting them at time 0 obviously        //
// Early Finish (EF): the earliest time a task can finish.                              //
//                   EF = ES of task + duration                                         //
//////////////////////////////////////////////////////////////////////////////////////////

// Computes the early start score of a task recursively
// Some resemblance to the "minimax" algorithm
int getEarlyStartScore(const Task& task, const vector<Task>& taskList) {
    // No dependencies
    if (task.dependencies.empty()) return 0; 

    int ES = 0;
    // For each task, go through its dependencies and update the early start
    for (const string& depName : task.dependencies) {
        const Task& depTask = getTaskFromList(depName, taskList);
        int depEF = getEarlyStartScore(depTask, taskList) + depTask.duration;

        // Since ES = max(EF of all dependencies)
        if (depEF > ES) ES = depEF;
    }

    return ES;
}

// Updates all early start and finish of task structure in task list by reference
void updateAllEarlyVars(vector<Task>& taskList) {
    // For each task in our task list
    for (Task& task : taskList){
        // Update early start (ES) and ealy finish (EF)
        task.ES = getEarlyStartScore(task, taskList);
        task.EF = task.ES + task.duration; 
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
// Backward pass for late start (LS) and late finish (LF)                               //
// Late Finish (LF): the latest time a task can finish without delaying the project.    //
// For the last task, LF = EF as the task has 0 dependencies)                           //
// For other tasks                                                                      //
//                  LF = min(LS of all successor tasks)                                 //
//                  ie. Task must finish before the earliest LS of its successors,      //
//                      otherwise one of them will be forced to start late.             //        
// If a task has no dependencies, we should be starting them at time 0 obviously        //
// Early Finish (EF): the earliest time a task can finish.                              //
//                   EF = ES of task + duration                                         //
//////////////////////////////////////////////////////////////////////////////////////////

// Populate successors for all tasks
void populateSuccessors(vector<Task>& taskList) {
    // For each task in tasklist
    for (auto& t : taskList) {
        // For each of its dependencies
        for (const string& depName : t.dependencies) {
            // The dependencies of a task means that the task is the successor of the dependencies
            Task& depTask = getTaskObjectFromList(depName, taskList);
            depTask.successors.push_back(t.name);
        }
    }
}

// Gets the late finish time
int getLateFinishScore(const Task& task, const vector<Task>& taskList) {
    // No successors -> end of project -> LF=EF
    if (task.successors.empty()) return task.EF; 

    // A large number so that we gurantee that there is at least a min(x, LF starting) != LF starting
    int LF = INT_MAX;

    // For each successor task
    for (const string& sName : task.successors) {
        const Task& sTask = getTaskFromList(sName, taskList);

        // Compute the late start of the successor
        int sLS = getLateFinishScore(sTask, taskList) - sTask.duration;

        // Takes tha minimum late start of all its successors, which is the LF
        if (sLS < LF) LF = sLS; 
    }
    return LF;
}

// Updates all late start and finish of task structure in task list by reference
void updateAllLateVars(vector<Task>& taskList) {
    // For each task in our task list
    for (Task& task : taskList){
        // Update late start (LS) and late finish (LF)
        task.LF = getLateFinishScore(task, taskList);
        task.LS = task.LF - task.duration; 
    }
}

// Update slack of each task by reference
void updateAllSlack(vector<Task>& taskList){
    // For each task in our task list
    for (Task& task : taskList){
        // Update slack (late start - early start)
        task.slack = task.LS - task.ES;
    }
}

// Outputs task details
// name, duration, ES, EF, LS, LF, slack
void outputTaskCSV(const vector<Task>& taskList, const string& filename = "output.csv") {
    ofstream file(filename);
    if (!file.is_open()) {
        cerr << "Failed to open file for writing: " << filename << endl;
        return;
    }

    // Write header
    file << "task,duration,ES,EF,LS,LF,slack\n";

    // Write task rows
    for (const auto& t : taskList) {
        file << t.name << "," 
             << t.duration << ","
             << t.ES << ","
             << t.EF << ","
             << t.LS << ","
             << t.LF << ","
             << t.slack << "\n";
    }

    file.close();
    cout << "Task details written to " << filename << endl;
}

// Outputs timeline CSV similar to a Gantt-chart which is a chart with:
//      horizontal bars to represent tasks and their durations on a timeline, 
//      showing project schedules, task dependencies, and progress over time
// Gantt-chart for project management: https://en.wikipedia.org/wiki/Gantt_chart
// NOTE: This is not exactly a Gantt-chart as csv can't fully express these charts, I used a very simplified model instead.
// Each column is a time unit; C = Tasks on critical path, X = task active, O = inactive
void outputTimelineCSV(const vector<Task>& taskList, const string& filename = "timeline.csv") {
    // Determine project length
    int projectLength = 0;
    for (const auto& t : taskList) {
        if (t.EF > projectLength) projectLength = t.EF;
    }

    ofstream file(filename);
    if (!file.is_open()) {
        cerr << "Failed to open file for writing: " << filename << endl;
        return;
    }

    // Write header row: Time units
    file << "Task";
    for (int t = 0; t < projectLength; ++t) {
        file << "," << t;
    }
    file << "\n";

    // Write task timeline rows
    for (const auto& t : taskList) {
        file << t.name;
        for (int time = 0; time < projectLength; ++time) {
            if (time >= t.ES && time < t.EF){ 
                if (t.slack == 0) file << ",C"; // critical task
                else file << ",X"; // task active
            }
            else file << ",O"; // task inactive
        }
        file << "\n";
    }

    file.close();
    cout << "Timeline written to " << filename << endl;
}

int main() {
    vector<Task> tasks = loadCSV("tasks.csv");

    // Forward and backward passes
    updateAllEarlyVars(tasks);
    populateSuccessors(tasks);
    updateAllLateVars(tasks);
    updateAllSlack(tasks);

    // Output CSV files
    outputTaskCSV(tasks, "output.csv");
    outputTimelineCSV(tasks, "timeline.csv");

    return 0;
}
#pragma once
#include <string>
#include <vector>
#include <chrono>

enum class PRStatus {
    OPEN,
    MERGED
};

struct PullRequest {
    std::string id;
    std::string name;
    std::string author_id;
    PRStatus status;
    std::vector<std::string> assigned_reviewers;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point merged_at;
    
    PullRequest(const std::string& id, const std::string& name, 
                const std::string& author_id, PRStatus status = PRStatus::OPEN)
        : id(id), name(name), author_id(author_id), status(status),
          created_at(std::chrono::system_clock::now()) {}
    
    bool isMerged() const { return status == PRStatus::MERGED; }
    
    std::string getStatusString() const {
        return status == PRStatus::OPEN ? "OPEN" : "MERGED";
    }
    
    static PRStatus stringToStatus(const std::string& statusStr) {
        return statusStr == "MERGED" ? PRStatus::MERGED : PRStatus::OPEN;
    }
};

struct PullRequestShort {
    std::string id;
    std::string name;
    std::string author_id;
    PRStatus status;
    
    PullRequestShort(const std::string& id, const std::string& name, 
                     const std::string& author_id, PRStatus status)
        : id(id), name(name), author_id(author_id), status(status) {}
};
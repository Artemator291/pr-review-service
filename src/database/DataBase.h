#pragma once
#include <memory>
#include <string>
#include <vector>
#include <libpq-fe.h>
#include "../models/User.h"
#include "../models/PullRequest.h"

class Database {
public:
    static Database& getInstance();
    
    bool connect(const std::string& connectionString);
    void disconnect();
    bool isConnected() const;
    
    bool createTeam(const Team& team);
    std::unique_ptr<Team> getTeam(const std::string& teamName);
    bool teamExists(const std::string& teamName);
    
    bool createOrUpdateUser(const User& user);
    bool setUserActive(const std::string& userId, bool isActive);
    std::unique_ptr<User> getUser(const std::string& userId);
    std::vector<User> getActiveTeamMembers(const std::string& teamName, const std::string& excludeUserId = "");
    
    bool createPullRequest(const PullRequest& pr);
    bool mergePullRequest(const std::string& prId);
    std::unique_ptr<PullRequest> getPullRequest(const std::string& prId);
    bool updatePRReviewers(const std::string& prId, const std::vector<std::string>& reviewers);
    std::vector<PullRequest> getPRsByReviewer(const std::string& userId);
    bool isPRMerged(const std::string& prId);
    bool prExists(const std::string& prId);
    bool bulkDeactivateUsers(const std::vector<std::string>& userIds);
std::vector<std::pair<std::string, std::string>> getOpenPRsWithReviewer(const std::string& reviewerId);

private:
    Database() = default;
    PGconn* connection_ = nullptr;
    
    int getTeamId(const std::string& teamName);
    std::string timeToString(const std::chrono::system_clock::time_point& time);
};
#pragma once
#include <vector>
#include <string>
#include <random>
#include <algorithm>
#include <stdexcept>
#include "../database/Database.h"
#include <User.h>

class ReviewAssignmentService {
public:
    ReviewAssignmentService(Database& db) : database_(db) {}
    
    std::vector<std::string> assignReviewers(const std::string& authorId, const std::string& teamName);
    std::string reassignReviewer(const std::string& prId, const std::string& oldReviewerId);
    
private:
    Database& database_;
    std::random_device random_device_;
    std::mt19937 generator_{random_device_()};
    
    std::vector<std::string> selectRandomReviewers(const std::vector<User>& candidates, int count);
};
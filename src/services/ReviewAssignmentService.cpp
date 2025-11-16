#include "ReviewAssignmentService.h"

std::vector<std::string> ReviewAssignmentService::assignReviewers(
    const std::string& authorId, const std::string& teamName) {
    
    auto candidates = database_.getActiveTeamMembers(teamName, authorId);
    return selectRandomReviewers(candidates, 2);
}

std::string ReviewAssignmentService::reassignReviewer(
    const std::string& prId, const std::string& oldReviewerId) {
    
    if (database_.isPRMerged(prId)) {
        throw std::runtime_error("Cannot reassign reviewers for merged PR");
    }
    
    auto oldReviewer = database_.getUser(oldReviewerId);
    if (!oldReviewer) {
        throw std::runtime_error("Reviewer not found");
    }
    
    auto pr = database_.getPullRequest(prId);
    if (!pr) {
        throw std::runtime_error("PR not found");
    }
    
    auto it = std::find(pr->assigned_reviewers.begin(), pr->assigned_reviewers.end(), oldReviewerId);
    if (it == pr->assigned_reviewers.end()) {
        throw std::runtime_error("Reviewer is not assigned to this PR");
    }
    
    auto candidates = database_.getActiveTeamMembers(oldReviewer->team_name, oldReviewerId);
    if (candidates.empty()) {
        throw std::runtime_error("No active replacement candidate in team");
    }
    
    auto newReviewers = selectRandomReviewers(candidates, 1);
    if (newReviewers.empty()) {
        throw std::runtime_error("No active replacement candidate in team");
    }
    
    return newReviewers[0];
}

std::vector<std::string> ReviewAssignmentService::selectRandomReviewers(
    const std::vector<User>& candidates, int count) {
    
    std::vector<std::string> selected;
    if (candidates.empty()) {
        return selected;
    }
    
    std::vector<User> available = candidates;
    std::shuffle(available.begin(), available.end(), generator_);
    
    int toSelect = std::min(count, static_cast<int>(available.size()));
    for (int i = 0; i < toSelect; i++) {
        selected.push_back(available[i].id);
    }
    
    return selected;
}
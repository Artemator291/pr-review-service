CREATE TABLE IF NOT EXISTS teams (
    id SERIAL PRIMARY KEY,
    name VARCHAR(255) UNIQUE NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS users (
    id VARCHAR(255) PRIMARY KEY,
    username VARCHAR(255) NOT NULL,
    team_id INTEGER REFERENCES teams(id),
    is_active BOOLEAN DEFAULT true,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS pull_requests (
    id VARCHAR(255) PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    author_id VARCHAR(255) REFERENCES users(id),
    status VARCHAR(50) DEFAULT 'OPEN',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    merged_at TIMESTAMP
);

CREATE TABLE IF NOT EXISTS pr_reviewers (
    id SERIAL PRIMARY KEY,
    pr_id VARCHAR(255) REFERENCES pull_requests(id) ON DELETE CASCADE,
    reviewer_id VARCHAR(255) REFERENCES users(id),
    assigned_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(pr_id, reviewer_id)
);

CREATE INDEX IF NOT EXISTS idx_users_team_id ON users(team_id);
CREATE INDEX IF NOT EXISTS idx_users_active ON users(is_active);
CREATE INDEX IF NOT EXISTS idx_pr_status ON pull_requests(status);
CREATE INDEX IF NOT EXISTS idx_pr_author ON pull_requests(author_id);
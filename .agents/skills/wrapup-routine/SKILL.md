---
name: wrapup-routine
description: End-of-session housekeeping routine for wrapping up a development session, performing security reviews, updating CHANGELOG.md/README.md/plan.md, generating chat diary entries, and requesting user confirmation for git commit and push. Trigger on mentions of 'wrap up', 'wrapup', 'session wrap', 'final review', 'housekeeping', 'finish session', or 'end session'.
---

# End-of-Session Housekeeping Routine

Execute the following steps whenever wrapping up a work session or upon explicit user request:

1. **Security Review**: Perform a security review of all changes made during the session to ensure no secrets, sensitive credentials (API keys, Wi-Fi credentials, tokens), or security vulnerabilities are introduced.
2. **Update Documentation**:
   - Update `CHANGELOG.md` with a concise summary of updates and changes under `[Unreleased]`.
   - Update `README.md` if any project usage, dependencies, setup instructions, or architectural details changed.
3. **Chat Diary Entry**: Follow all instructions in [.diary_prompt.md](file:///.diary_prompt.md) to create an entry for the chat session in `CHAT_DIARY.md`.
4. **Update Plan for Major Changes**: Update `plan.md` if any major architectural or system design changes were made during the session.
5. **User Confirmation for Git Commit & Push**:
   - Ask the user for explicit permission before committing changes and pushing them to GitHub.

# Project Rules & Customizations

## End-of-Session Housekeeping Routine

At the end of a work session (or whenever the user requests session wrapping / wrap up / final review / housekeeping):

1. **Security Review**: Perform a security review of all changes made during the session to ensure no secrets, sensitive credentials (API keys, Wi-Fi credentials, tokens), or security vulnerabilities are introduced.
2. **Update Documentation**:
   - Update `CHANGELOG.md` with a concise summary of updates and changes.
   - Update `README.md` if any project usage, dependencies, setup instructions, or architectural details changed.
3. **Chat Diary Entry**: Follow all instructions in [.diary_prompt.md](file:///.diary_prompt.md) to create an entry for the chat session.
4. **User Confirmation for Git Commit & Push**:
   - Ask the user for explicit permission before committing changes and pushing them to GitHub.




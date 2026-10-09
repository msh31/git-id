# git-id
A simple hook for git that stops you from pushing commits that use the wrong identity

## What is this?
git-id is a pre-push hook that checks the author and committer emails of outgoing commits against the owner of the repository in the remote URL.

Example:
You have your user account and own an organization and you do not want either identity to accidentally leak into the other, this pre-push hook will block
the outgoing commit(s)

---

## Install
Simply run ``git-id install`` within a repository and the hook will be installed automatically!

You may also want to enforce this globally, in which case you can simply pass the ``--global`` flag to the previous command.
> [!NOTE]
Applying this globally will prevent any pre-existing hooks from running

## Usage
1. Create a config here: ``~/.git-identities``
2. Setup your identities, an example entry looks like this:  
``owner1|owner2: user@mail.com, user@second-mail.com``
3. Use git and once a mis-match occurs, the hook will catch it

---

## Troubleshooting
* How do I bypass this hook?
	* You can simply run ``git push --no-verify`` and the hook will be skipped
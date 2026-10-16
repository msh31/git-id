# git-id
A simple hook for git that stops you from pushing commits that use the wrong identity

![Windows](https://img.shields.io/badge/Windows-blue)
![FreeBSD](https://img.shields.io/badge/FreeBSD-red?logo=freebsd)

---

## What is this?
git-id is a pre-push hook that blocks outgoing commits by checking the author and committer emails of against the owner of the repository in the remote URL.

Example:
You define ``my-org: org@example.com`` in your config. You commit with ``me@personal.com`` and push to ``github.com/my-org/repo``. git-id lists the outgoing author mails and blocks the push until you fix the identity

> [!WARNING]
This does NOT hide your account from GitHub's activity feed, which shows who pushed

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
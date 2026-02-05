# Publishing this project to GitHub

## 1. Create a new repository on GitHub

1. Go to [github.com](https://github.com) and sign in.
2. Click **New repository** (or **+** → **New repository**).
3. Choose a name (e.g. `VulkanProjects` or `vulkan-app`).
4. Optionally add a description, choose **Public**, and do **not** add a README, .gitignore, or license (this project already has them).
5. Click **Create repository**.

## 2. Initialize Git and push (first time)

In the project root (where `CMakeLists.txt` and `.gitignore` are), run:

```bash
# Initialize Git (if not already)
git init

# Add the GitHub repo as remote (replace YOUR_USERNAME and YOUR_REPO with your GitHub username and repo name)
git remote add origin https://github.com/YOUR_USERNAME/YOUR_REPO.git

# Stage all files (respects .gitignore)
git add .

# First commit
git commit -m "Initial commit: Vulkan app with SDL3, cross-platform setup"

# Push to GitHub (main branch; use -u so future git push works without arguments)
git branch -M main
git push -u origin main
```

If GitHub asks for authentication, use a **Personal Access Token** (Settings → Developer settings → Personal access tokens) as the password when using HTTPS, or set up **SSH** and use the SSH URL: `git@github.com:YOUR_USERNAME/YOUR_REPO.git`.

## 3. Later updates

After changing code:

```bash
git add .
git commit -m "Short description of what you changed"
git push
```

## 4. Optional: SSH instead of HTTPS

To use SSH so you don’t need a token for every push:

1. [Add an SSH key to your GitHub account](https://docs.github.com/en/authentication/connecting-to-github-with-ssh/adding-a-new-ssh-key-to-your-github-account).
2. When adding the remote, use the SSH URL:
   ```bash
   git remote add origin git@github.com:YOUR_USERNAME/YOUR_REPO.git
   ```
   (If you already added `origin` with HTTPS, change it with: `git remote set-url origin git@github.com:YOUR_USERNAME/YOUR_REPO.git`.)

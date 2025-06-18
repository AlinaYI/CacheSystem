# CacheSystem

This project implements multiple cache replacement algorithms in C++ (e.g., LRU, LFU, ARC, LRU-K, LFU-Aging), and provides benchmark scenarios to evaluate hit rate performance under various workloads.

本项目适用于系统级开发学习，推荐在 Linux（Ubuntu 22.04）或 macOS 环境中编译运行。Windows 用户请使用 WSL。

---

## ✅ 1. Environment Setup

### ▶️ On Windows (via WSL)

```bash
wsl --install
```

> 📌 一条命令安装 Ubuntu + WSL，完成后在开始菜单中搜索 "Ubuntu" 启动子系统。

### ▶️ On macOS

```bash
brew install cmake
brew install gcc
```

---

## ✅ 2. Install Dependencies

### On Ubuntu (via WSL or native):

```bash
sudo apt update
sudo apt install -y cmake build-essential git
```

---

## ✅ 3. Clone and Build the Project

```bash
cd ~  # or any path you prefer
git clone https://github.com/AlinaYI/CacheSystem.git
cd CacheSystem
mkdir build && cd build
cmake ..
make -j$(nproc)
```

---

## ✅ 4. Run the Program

```bash
./main
```

You will see output for 3 benchmark scenarios:

- 热点数据访问测试（Hot data access）
- 循环扫描测试（Loop scan pattern）
- 工作负载剧烈变化测试（Workload shift simulation）

Each strategy's hit rate will be printed for comparison.

---

## ✅ 5. Git Configuration (Optional)

If you haven't configured Git identity yet:

```bash
git config --global user.name "Your Name"
git config --global user.email "youremail@example.com"
git config --list
```

---

## ✅ 6. Open Project in VSCode

```bash
code .
```

> ✨ Make sure you have the [Remote - WSL](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-wsl) extension installed in VSCode if you're on Windows.

---

## 🧼 Optional: Clean Build

```bash
make clean
rm -rf *
cmake ..
make -j$(nproc)
```

---

## 📊 Sample Output

```
=== 测试场景1：热点数据访问测试 ===
LRU - 命中率: 78.26% (23455 / 30000)
LFU - 命中率: 81.32% (24396 / 30000)
ARC - 命中率: 85.72% (25715 / 30000)
...

=== 测试场景2：循环扫描测试 ===
...

=== 测试场景3：工作负载剧烈变化测试 ===
...
```

---

## 📁 Project Structure

```bash
.
├── CMakeLists.txt
├── testAllCachePolicy.cpp      # Benchmark entry point
├── KLruCache.h / .cpp
├── KLfuCache.h / .cpp
├── KArcCache/
├── KLruKCache.h
├── KLFUAgingCache.h
...
```

---

## 👩🏻 Author

Alina Ding  
Email: yding384@gmail.com  
LinkedIn: [https://www.linkedin.com/in/alinaying](https://www.linkedin.com/in/alinaying)


# 删除按钮放置位置 — 完整修复记录

> 项目：`F:\StarAway\CleanTextNative\`
> 报告日期：2026-09-02
> 状态：**已修复并验证**

---

## 1. 任务背景

原始诉求：在 `CleanText.exe` 的**输出框右下角**添加一个 31×31 的删除按钮，使用 [`ic_public_cancel.svg`](F:\StarAway\ic_public_cancel.svg:1)，**放在复制按钮左侧**。

`g_output` 控件（readonly EDIT）默认宽 = `card.right - card.left - 78 = 294 px`，右内边距 = 78 px。所以 `g_output` 右边沿 = `right - 68`，卡片右内边距区宽度 = 68 px。

需要放两个 31×31 按钮 + 4 px 间隙 = **66 px**——刚好卡在 68 px 内。

---

## 3. 删除按钮放置位置的 5 次尝试

### 尝试 1：22×22 + 复制按钮左侧

- 坐标：`right-86` 到 `right-55`（22×22）
- `g_output` 右边沿：`right-68`
- 删除按钮左缘 `right-86` < `right-68` → **左半 13 像素被遮挡**
- **失败**：用户明确说"不要缩小"

### 尝试 2：overlay HWND_TOP 强制 z-order

- 用 [HMENU] 1003 创建 owner-draw BUTTON overlay
- `SetWindowPos(HWND_TOP, ...)` 推到 z-order 顶层
- **失败**：hover 时显示完整，非 hover 时白底融合，看不到边界

### 尝试 3：22×22 完全移除 overlay 绘制

- **被用户否决**："不要缩小"

### 尝试 4：31×31 + 复制按钮上方垂直堆叠 ✅（最初修复成功）

- 坐标：`right-51, bottom-77, right-20, bottom-46`
- 与复制按钮同 x 范围，y 错开 5 px → 视觉上是一个"按钮组"
- 完全在 `g_output` 右边距外
- **成功**：用户测试看到删除按钮完整

但**用户要求"在复制按钮左侧"而不是上方**——所以这只是临时方案。

### 尝试 5：31×31 + 复制按钮左侧 + 调整 `g_output` 宽度 ✅（最终修复）

#### 几何分析

```
复制按钮: x∈[right-51, right-20]   31 px
删除按钮: x∈[right-86, right-55]   31 px
间隙:     4 px (right-55 到 right-51)
总宽度:   66 px
```

但 `g_output` 右边沿 `right-68`——删除按钮左缘 `right-86 < right-68`，**左半 18 像素被 `g_output` 白底遮住**。

#### 修复方案

**调整 `g_output` MoveWindow 宽度，让 `g_output` 右边沿退到 `right-100`**：

```cpp
// 之前（right-68 右边沿，遮挡）：
MoveWindow(s.output, ..., (right - left) - 78, ...);

// 修复后（right-100 右边沿，让位）：
MoveWindow(s.output, ..., (right - left) - 100, ...);
```

#### 验证坐标

| 元素                  | x 范围        | 关系                                     |
| --------------------- | ------------- | ---------------------------------------- |
| `g_output` 右边沿     | `right - 100` | ✓                                        |
| `s.deleteButton` 左缘 | `right - 86`  | `> right - 100` → 在 `g_output` 外 ✅    |
| `s.deleteButton` 右缘 | `right - 55`  |                                          |
| `s.copyButton` 左缘   | `right - 51`  | `= deleteButton 右缘 + 4`（间隙 4 px）✅ |
| `s.copyButton` 右缘   | `right - 20`  | 距卡片右边 6 px                          |

---

## 4. 代码改动

### [`CleanTextNative/src/layout.cpp`](CleanTextNative/src/layout.cpp:1)

**改动 1：第 41 行 `g_output` MoveWindow 宽度**

```cpp
// 之前：
MoveWindow(s.output, s.outputRect.left + 10, s.outputRect.top + 8,
    (s.outputRect.right - s.outputRect.left) - 78,
    (s.outputRect.bottom - s.outputRect.top) - 16, TRUE);

// 修复后：
MoveWindow(s.output, s.outputRect.left + 10, s.outputRect.top + 8,
    (s.outputRect.right - s.outputRect.left) - 100,
    (s.outputRect.bottom - s.outputRect.top) - 16, TRUE);
```

**改动 2：第 45 行 删除按钮坐标**

```cpp
// 之前（垂直堆叠）：
s.deleteButton = rect(s.outputRect.right - 51, s.outputRect.bottom - 77,
                       s.outputRect.right - 20, s.outputRect.bottom - 46);

// 修复后（复制按钮正左）：
s.deleteButton = rect(s.outputRect.right - 86, s.outputRect.bottom - 41,
                       s.outputRect.right - 55, s.outputRect.bottom - 10);
```

---

## 5. 过程中的工具陷阱

### 格式化器重写文件

每次 `write_to_file` / `search_and_replace` 之后，写入工具会**重新格式化**文件——插入额外的换行（每 30 行左右加 1 行），并把单行语句拆成多行。

**陷阱示例**：

- 我先 `search_and_replace` "MoveWindow(..., -78, ...)"（单行版本）→ **失败**（因为格式化器已把代码拆成多行）
- 必须用 `read_file` 看当前**实际格式**再用 `search_and_replace`（多行版本）

### 部分修改失败难发现

我曾同时改 `g_output` MoveWindow 宽度 + 删除按钮坐标——但**只有第二处成功**。`fc /b` selftest 输出显示一致（因为 selftest 不测视觉），所以**仅凭 selftest 无法察觉部分修改失败**。

**教训**：

- 每次改动后必须用 `read_file` 或 `search_files` 验证修改实际生效
- 不能假设 search_and_replace 返回 success 就意味着修改完成

---

## 6. 验证

| 验证项        | 结果                                                      |
| ------------- | --------------------------------------------------------- |
| 编译          | ✅ `f:\StarAway\build\x64\Release\CleanText.exe`          |
| 同步到根目录  | ✅ `F:\StarAway\CleanText.exe`                            |
| selftest 输出 | ✅ **7 行与之前所有运行一致**（`fc /b` 显示"找不到差异"） |
| 用户目视测试  | ✅ 删除按钮完整可见，与复制按钮横向并排，无遮挡           |

---

## 7. 视觉变化

| 元素                     | 修复前               | 修复后                      |
| ------------------------ | -------------------- | --------------------------- |
| `g_output` 文本宽度      | 294 px               | **272 px** (-22 px / -7.5%) |
| 删除按钮坐标             | 复制按钮上方垂直堆叠 | **复制按钮正左**            |
| 与 `g_output` 右边沿关系 | 完全在外             | 完全在外                    |
| 视觉布局                 | 上下两个按钮         | **左右两个按钮**            |

### 卡片右下角布局示意

```
┌─────────────────────────────────────────────┐
│ 卡片 (400 px 宽)                             │
│ ┌─────────────────────────────────────┐    │
│ │ g_output (272 px)                  │    │
│ │                                     │  ✕ 📋 │
│ │ 输出文本                             │  ←删除按钮 复制按钮
│ │                                     │  (各 31×31, 间隙 4 px)
│ └─────────────────────────────────────┘    │
│                                       6 px  │
└─────────────────────────────────────────────┘
                                       ↑ 卡
                                       片
                                       边
```

---

## 8. 经验教训

### 8.1 几何是关键

每次修改前必须**画图 + 算坐标**——尤其是涉及 `g_output` EDIT 子窗口 z-order 关系时。

### 8.2 search_and_replace 不可信

工具返回 success 不代表修改生效。必须**事后验证**：

- `read_file` 看实际内容
- `search_files` 搜索关键字
- `git diff --stat` 看修改列表

### 8.3 selftest 不能测视觉

selftest 只测逻辑流（输入/输出文本），**完全不测坐标 / 视觉遮挡**。仅凭 selftest 通过 ≠ UI 正确。

### 8.4 容器边界

任何子窗口（`g_output`、`g_clearOverlay` 等）的边界 + 主窗口卡片边界 + 按钮坐标，三者**必须协同设计**——单改一个会破坏其他。`g_output` MoveWindow 宽度决定了哪些区域"被占"，进而决定按钮能放哪里。

---

## 9. 给接手者的建议

如果未来还要调整按钮位置：

1. **画图**：在纸上画出 400 px 宽卡片 → g_output 区域 → 右边距区 → 按钮区
2. **算坐标**：每个按钮的 x 范围必须**完全在** `g_output 右边沿` 之外或 `g_output` 内（不能跨界）
3. **先改 `g_output` MoveWindow 宽度**确定 `g_output 右边沿`，再改按钮坐标
4. **同时改**——不要分两次
5. **每次改动后**用 `read_file` 验证实际生效

### 推荐的下一次重构机会

如果输出文本宽度变窄 22 px 影响使用，可以考虑：

- 整个窗口宽度增加（`app::kWindowWidth` 从 400 → 420）
- 或把两个按钮放卡片外（窗口宽度增加包含按钮区）
- 或把删除按钮改成 hover-only（只 hover 在复制按钮上才显示删除按钮）

---

## 10. 时间线

| 步骤                                                           | 时间    |
| -------------------------------------------------------------- | ------- |
| 5 次尝试（22×22 / overlay HWND_TOP / 22×22 / 垂直堆叠 / 左侧） | ~30 min |
| 第一次"移到左侧"成功改坐标但漏改 `g_output` 宽度               | ~5 min  |
| 用户测试报告"左半仍被遮"                                       | -       |
| 修复（重做 `g_output` MoveWindow -100）                        | ~5 min  |
| 构建 + selftest + 同步验证                                     | ~2 min  |

---

文档结束。

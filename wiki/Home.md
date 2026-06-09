<p align="center">
  <img src="images/SS-midle-gray.png" alt="PolySmith" style="width: 100%; max-width: 100%;">
</p>

<h1 align="center">PolySmith Wiki</h1>

<p align="center" style="font-size: 1.1em; font-weight: 600; letter-spacing: 0.03em;">Local-first desktop CAD for hobbyist 3D-printing workflows</p>


---

### 🏠 Getting Started

| | |
|---|---|
| 📋 **[Project Summary](Project-Summary)** | High-level overview: architecture, shipped features, mantra, UX pattern, roadmap, and contribution rules |

## 📖 Pages

### 📖 Reference

| | |
|---|---|
| 📚 **[Glossary](Glossary)** | Canonical terminology — use these definitions in commits, bugs, and design discussions |

### 🏛️ Architecture & Design

| | |
|---|---|
| 🏗️ **[Architecture Overview](Architecture-Overview)** | High-level system architecture: UI, Tauri, C++ core, IPC |
| 🧭 **[Core-UI Design Principles](Core-UI-Design-Principles)** | **Binding.** Core sends document state, not interaction state. What belongs where. |
| 🗺️ **[Repository Map](Repository-Map)** | Top-level directory structure and ownership rules |
| 📡 **[IPC Protocol](IPC-Protocol)** | JSON-based protocol between UI and CAD core |
| 🔷 **[Topological Naming Problem](Topological-Naming-Problem)** | TNP strategy and defences (project mantra) |
| 🎨 **[Design System](Design-System)** | Dark/Light dual theme system and Catppuccin palette support |
| 📏 **[Dimension Rendering Design](Dimension-Rendering-Design)** | Design rationale and decision record for dimension rendering |
| 📐 **[Display Units](Display-Units)** | Metric / inch toggle architecture |
| 📋 **[Contextual Modeling Workflow](Contextual-Modeling-Workflow)** | Binding UX pattern for all modeling features |
| ⚡ **[Planegcs Dual-Deployment Solver](Planegcs-Dual-Solver)** | WASM + native planegcs for UI-side drag preview and core-side final solve |
| 🧩 **[GCS Implementation Strategy](GCS-Implementation-Strategy)** | Freeze state machine, speculative inferencing, DOF feedback — implementation plan |
| 🔧 **[CAM Development Plan](CAM-Development)** | CAM workspace: data model, TNP strategy, preview pipeline, v1 operations |

### 🤖 AI / Agent

| | |
|---|---|
| 🧠 **[AI CAD Command Language](AI-CAD-Command-Language)** | Full command reference for AI agents interacting with PolySmith |
| 📜 **[Codex Rules](Codex-Rules)** | AI workflow rules for development |
| 📝 **[Task Templates](Task-Templates)** | Templates for AI-assisted implementation tasks |

### 🛣️ Roadmap & Tracking

| | |
|---|---|
| 🎯 **[V1 Roadmap](V1-Roadmap)** | Project focus and near-term milestones |
| 📋 **[Implementation Log](Implementation-Log)** | Running log of shipped implementation milestones |

### 🧩 Sketch System

| | |
|---|---|
| 🏗️ **[2D Sketch System Architecture](2D-Sketch-System-Architecture)** | As-built constraints, snapping, and unified selection filter |
| ✂️ **[Trim Tool — Implementation Plan](Trim-Tool-Implementation-Plan)** | Shipped. Reference documentation for the trim engine design. |
| 🔤 **[Text Tool — Implementation Plan](Text-Tool-Implementation-Plan)** | Planned. Text as sketch entities: font-to-BRep, extrusion, emboss/deboss. |
| 📏 **[Draft Dimension Visualization](Draft-Dimension-Visualization)** | Shipped. Scene-rendered dimension lines and arcs for sketch draft tools. |
| 🔧 **[Sketch Tool Implementation](Sketch-Tool-Implementation)** | Guide for adding new drawable sketch shapes |
| ↕️ **[Sketch Endpoint Drag](Sketch-Endpoint-Drag)** | Endpoint drag with planegcs WASM solver and constraint behaviour |

### 📖 Help (User Documentation)

| | |
|---|---|
| ✂️ **[Trim Tool](help/trim.md)** | How to activate and use the trim tool |
| 📏 **[Line Tool](help/line.md)** | How to use the line tool |
| ⭕ **[Circle Tool](help/circle.md)** | How to use the circle tool |
| ⬜ **[Rectangle Tool](help/rectangle.md)** | How to use the rectangle tool |
| ⚙️ **[Parameters](help/parameters.md)** | How to use the parameter system |

### 🏛️ Decision Records

| | |
|---|---|
| 📋 **[ADR 0001: Initial Tech Stack](ADR-0001-Tech-Stack)** | Tech stack decisions: React, Tauri, C++, OCCT |

---



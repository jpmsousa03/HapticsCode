# Tamagotchi-Style Haptic Pet Game — Technical Plan

## Overview

Transform `haptic_gui.html` into a Tamagotchi-style virtual pet game that controls the haptic robot. The robot becomes a "pet" that responds emotionally to player actions.

---

## 1. Architecture Analysis

### Current System
- **4 emotion presets**: Delighted (0), Unhappy (1), Relaxed (5), Angry (8)
- **Serial communication**: Web Serial API at 250000 baud
- **Animation detection**: JSON events `preset_start` and `preset_done` from firmware
- **Touch detection**: Firmware detects physical interaction and triggers preset

### Key Integration Points
```
GUI Button Press → Serial "P <id>" + "T" → Firmware triggers animation
                                        → Firmware sends {"event":"preset_start","id":X}
                                        → Firmware sends {"event":"preset_done"}

Physical Touch → Firmware detects → Firmware sends {"event":"preset_start","id":X}
```

---

## 2. Game Design

### 2.1 Pet Status System

| Status Bar | Description | Decay Rate | Color |
|------------|-------------|------------|-------|
| **Hunger** | How full the pet is | -1/30s | `#ff9f47` (orange) |
| **Happiness** | Overall mood | -1/45s | `#e8ff47` (yellow) |
| **Energy** | Activity level | -1/60s | `#47ffe8` (cyan) |
| **Cleanliness** | Hygiene state | -1/90s | `#ff47a3` (pink) |

Each bar: 0-100, displayed as horizontal progress bars with animated fill.

### 2.2 Action Button Mapping

| Emotion | Animation | Button Theme | Cycling Actions |
|---------|-----------|--------------|-----------------|
| **Relaxed** (5) | Soothing | 💤 Rest/Comfort | "Pet", "Lullaby", "Blanket", "Cuddle" |
| **Delighted** (0) | Exciting | 🎉 Play/Treat | "Treat", "Play Ball", "Tickle", "Dance" |
| **Unhappy** (1) | Sad | 💔 Neglect | "Ignore", "Scold", "Take Toy", "Bedtime" |
| **Angry** (8) | Annoying | 😤 Annoy | "Poke", "Wake Up", "Loud Noise", "Tease" |

### 2.3 Status Effects per Action

```javascript
const ACTION_EFFECTS = {
  // Relaxed actions - soothing
  "Pet":       { energy: +5, happiness: +3 },
  "Lullaby":   { energy: +10, happiness: +2 },
  "Blanket":   { energy: +8, cleanliness: -2 },
  "Cuddle":    { happiness: +8, energy: +3 },
  
  // Delighted actions - exciting
  "Treat":     { hunger: +15, happiness: +5 },
  "Play Ball": { happiness: +10, energy: -8 },
  "Tickle":    { happiness: +12, energy: -5 },
  "Dance":     { happiness: +8, energy: -10 },
  
  // Unhappy actions - saddening
  "Ignore":    { happiness: -10 },
  "Scold":     { happiness: -15, energy: -5 },
  "Take Toy":  { happiness: -12 },
  "Bedtime":   { happiness: -5, energy: +15 },
  
  // Angry actions - annoying
  "Poke":      { happiness: -8, energy: -3 },
  "Wake Up":   { energy: -15, happiness: -5 },
  "Loud Noise":{ happiness: -10, energy: -8 },
  "Tease":     { happiness: -12, cleanliness: -3 }
};
```

---

## 3. UI/UX Design

### 3.1 Layout (Mobile-First, Responsive)

```
┌─────────────────────────────────────────────────────────┐
│  🐾 HAPTIC PET                          [●] connected   │  ← Header
├─────────────────────────────────────────────────────────┤
│                                                         │
│              ┌─────────────────────┐                    │
│              │    EMOJI STREAM     │  ← Floating emojis │
│              │    ↑ ↑ ↑ ↑ ↑       │                    │
│              └─────────────────────┘                    │
│                                                         │
│              ┌─────────────────────┐                    │
│              │   🔄 HEAD ICON      │  ← Rotating head   │
│              │      132.5°         │                    │
│              └─────────────────────┘                    │
│                                                         │
│  ┌──────────────────────────────────────────────────┐   │
│  │ 🍖 Hunger    ████████████░░░░░░░░  75%           │   │  ← Status bars
│  │ 😊 Happiness ██████████████░░░░░░  85%           │   │
│  │ ⚡ Energy    ████████░░░░░░░░░░░░  50%           │   │
│  │ ✨ Clean     ██████████████████░░  90%           │   │
│  └──────────────────────────────────────────────────┘   │
│                                                         │
│  ┌────────────────┐  ┌────────────────┐                 │
│  │  💤 RELAXED    │  │  🎉 DELIGHTED  │                 │  ← Action buttons
│  │    "Pet"       │  │   "Treat"      │                 │
│  └────────────────┘  └────────────────┘                 │
│  ┌────────────────┐  ┌────────────────┐                 │
│  │  💔 UNHAPPY    │  │  😤 ANGRY      │                 │
│  │   "Ignore"     │  │    "Poke"      │                 │
│  └────────────────┘  └────────────────┘                 │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### 3.2 Visual Style

**Color Palette** (Tamagotchi-inspired, modern):
```css
:root {
  --bg: #1a1a2e;           /* Deep purple-black */
  --panel: #16213e;        /* Navy panel */
  --border: #0f3460;       /* Dark blue border */
  --accent-relaxed: #47ffe8;
  --accent-delighted: #e8ff47;
  --accent-unhappy: #47a3ff;
  --accent-angry: #ff4747;
  --text: #eaeaea;
  --muted: #7f8c8d;
}
```

**Typography**:
- Keep existing `Syne` for headings (playful, modern)
- Keep `DM Mono` for data (technical feel)
- Consider adding `Nunito` or `Quicksand` for button labels (friendly)

### 3.3 Button Design

Each button should have:
1. **Emotion emoji** (large, top)
2. **Category label** (small, muted)
3. **Action text** (prominent, animated on change)
4. **Glow effect** matching emotion color
5. **Press animation**: scale down + ripple effect

```css
.action-btn {
  background: linear-gradient(145deg, var(--panel), var(--bg));
  border: 2px solid var(--emotion-color);
  border-radius: 16px;
  box-shadow: 0 0 20px var(--emotion-color-alpha);
  transition: transform 0.1s, box-shadow 0.2s;
}

.action-btn:active {
  transform: scale(0.95);
  box-shadow: 0 0 30px var(--emotion-color);
}
```

---

## 4. Animation Specifications

### 4.1 Emoji Stream Animation

When a button is pressed, spawn 5-8 emojis that float upward to the center-top.

```javascript
const EMOTION_EMOJIS = {
  relaxed:  ['💤', '😌', '🌙', '💆', '🧘', '☁️', '🌸'],
  delighted: ['🎉', '✨', '🌟', '💖', '🎊', '🥳', '🍬'],
  unhappy:  ['💔', '😢', '🥀', '💧', '🌧️', '😞', '💨'],
  angry:    ['😤', '💢', '🔥', '⚡', '💥', '😠', '🌋']
};
```

**Animation CSS**:
```css
@keyframes emoji-float {
  0% {
    transform: translateY(0) scale(1);
    opacity: 1;
  }
  100% {
    transform: translateY(-200px) scale(0.5);
    opacity: 0;
  }
}

.emoji-particle {
  position: absolute;
  font-size: 28px;
  animation: emoji-float 1.5s ease-out forwards;
  pointer-events: none;
}
```

### 4.2 Button Text Cycling Animation

```css
@keyframes text-swap {
  0% { transform: translateY(0); opacity: 1; }
  40% { transform: translateY(-20px); opacity: 0; }
  60% { transform: translateY(20px); opacity: 0; }
  100% { transform: translateY(0); opacity: 1; }
}

.action-text.changing {
  animation: text-swap 0.4s ease-in-out;
}
```

### 4.3 Status Bar Animations

```css
.status-fill {
  transition: width 0.5s ease-out;
  background: linear-gradient(90deg, var(--bar-color), var(--bar-color-light));
}

/* Pulse when low */
.status-fill.critical {
  animation: pulse-warning 1s infinite;
}

@keyframes pulse-warning {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.5; }
}
```

### 4.4 Button Press Ripple

```javascript
function createRipple(event, button) {
  const ripple = document.createElement('span');
  ripple.className = 'ripple';
  const rect = button.getBoundingClientRect();
  ripple.style.left = `${event.clientX - rect.left}px`;
  ripple.style.top = `${event.clientY - rect.top}px`;
  button.appendChild(ripple);
  setTimeout(() => ripple.remove(), 600);
}
```

```css
.ripple {
  position: absolute;
  border-radius: 50%;
  background: rgba(255, 255, 255, 0.4);
  transform: scale(0);
  animation: ripple-expand 0.6s ease-out;
  pointer-events: none;
}

@keyframes ripple-expand {
  to {
    transform: scale(4);
    opacity: 0;
  }
}
```

---

## 5. Audio System

### 5.1 Sound Files Required

Place in same directory as `haptic_gui.html`:
- `relaxed.mp3`
- `delighted.mp3`
- `unhappy.mp3`
- `angry.mp3`

### 5.2 Audio Implementation

```javascript
// Configuration - easily changeable
const AUDIO_CONFIG = {
  relaxed: { file: 'relaxed.mp3', volume: 0.7 },
  delighted: { file: 'delighted.mp3', volume: 0.8 },
  unhappy: { file: 'unhappy.mp3', volume: 0.6 },
  angry: { file: 'angry.mp3', volume: 0.75 }
};

// Preload audio
const audioCache = {};
Object.entries(AUDIO_CONFIG).forEach(([emotion, config]) => {
  audioCache[emotion] = new Audio(config.file);
  audioCache[emotion].volume = config.volume;
  audioCache[emotion].preload = 'auto';
});

// Play on animation trigger (both manual and robot-detected)
function playEmotionSound(emotion) {
  const audio = audioCache[emotion];
  if (audio) {
    audio.currentTime = 0;
    audio.play().catch(e => console.log('Audio play blocked:', e));
  }
}
```

### 5.3 Trigger Detection

**Manual trigger** (GUI button press):
```javascript
function triggerAction(emotionId) {
  // ... existing trigger code ...
  playEmotionSound(getEmotionName(emotionId));
}
```

**Robot-detected trigger** (physical touch):
```javascript
function parseRx(line) {
  // ... existing parse code ...
  if (obj.event === 'preset_start') {
    const emotionName = getEmotionName(obj.id ?? activePreset);
    playEmotionSound(emotionName);
    // ... rest of handling ...
  }
}
```

---

## 6. State Management

### 6.1 Game State Object

```javascript
const gameState = {
  stats: {
    hunger: 80,
    happiness: 75,
    energy: 60,
    cleanliness: 90
  },
  currentActions: {
    relaxed: 0,   // Index into action arrays
    delighted: 0,
    unhappy: 0,
    angry: 0
  },
  lastDecay: Date.now(),
  isAnimating: false
};
```

### 6.2 Decay System

```javascript
const DECAY_RATES = {
  hunger: { interval: 30000, amount: 1 },      // -1 every 30s
  happiness: { interval: 45000, amount: 1 },   // -1 every 45s
  energy: { interval: 60000, amount: 1 },      // -1 every 60s
  cleanliness: { interval: 90000, amount: 1 }  // -1 every 90s
};

function tickDecay() {
  const now = Date.now();
  Object.entries(DECAY_RATES).forEach(([stat, config]) => {
    // Decay logic
    gameState.stats[stat] = Math.max(0, gameState.stats[stat] - config.amount);
  });
  updateStatusBars();
}

// Run every 30 seconds
setInterval(tickDecay, 30000);
```

### 6.3 Action Cycling Logic

```javascript
function cycleAction(emotion) {
  const actions = ACTION_SETS[emotion];
  gameState.currentActions[emotion] = 
    (gameState.currentActions[emotion] + 1) % actions.length;
  
  // Animate text change
  const textEl = document.querySelector(`#btn-${emotion} .action-text`);
  textEl.classList.add('changing');
  setTimeout(() => {
    textEl.textContent = actions[gameState.currentActions[emotion]];
    textEl.classList.remove('changing');
  }, 200);
}

// Cycle related buttons after action
function onActionComplete(triggeredEmotion) {
  // Cycle the pressed button and 1-2 related buttons
  cycleAction(triggeredEmotion);
  
  // Cycle one other random button for variety
  const others = ['relaxed', 'delighted', 'unhappy', 'angry']
    .filter(e => e !== triggeredEmotion);
  cycleAction(others[Math.floor(Math.random() * others.length)]);
}
```

---

## 7. Critical Implementation Details

### 7.1 Animation Detection (Both Sources)

The system must detect animations from **two sources**:

1. **GUI-triggered**: User presses button → `sendCmd("T")` → firmware responds with `preset_start`
2. **Robot-triggered**: Physical touch → firmware auto-triggers → sends `preset_start`

Both paths converge at `parseRx()` when receiving `{"event":"preset_start","id":X}`.

```javascript
function parseRx(line) {
  try {
    const obj = JSON.parse(line);
    
    if (obj.event === 'preset_start') {
      const emotionId = obj.id ?? activePreset;
      const emotionName = PRESET_TO_EMOTION[emotionId];
      
      // Play sound (works for BOTH manual and robot-triggered)
      playEmotionSound(emotionName);
      
      // Show visual feedback
      showAnimationFeedback(emotionName);
      
      // Apply stat changes
      applyStatChanges(emotionName);
      
      // Spawn emoji stream
      spawnEmojiStream(emotionName);
    }
    
    if (obj.event === 'preset_done') {
      // Cycle button texts
      onActionComplete(currentEmotion);
    }
    
    // ... rest of existing parsing ...
  } catch (e) { /* ... */ }
}
```

### 7.2 Preset ID to Emotion Mapping

```javascript
const PRESET_TO_EMOTION = {
  0: 'delighted',
  1: 'unhappy',
  5: 'relaxed',
  8: 'angry'
};

const EMOTION_TO_PRESET = {
  delighted: 0,
  unhappy: 1,
  relaxed: 5,
  angry: 8
};
```

---

## 8. Self-Critical Analysis

### 8.1 Design Critique

| Aspect | Assessment | Industry Standard? | Recommendation |
|--------|------------|-------------------|----------------|
| **Color scheme** | Dark theme with neon accents | ✅ Modern gaming aesthetic | Keep, matches cyberpunk/retro-future trend |
| **Button layout** | 2x2 grid | ✅ Standard for 4-action games | Good, thumb-friendly on mobile |
| **Status bars** | Horizontal with icons | ✅ Universal game UI pattern | Add subtle animations for polish |
| **Typography** | Mixed fonts | ⚠️ Could be cleaner | Consider single font family with weights |
| **Emoji stream** | Floating particles | ✅ Common in mobile games | Ensure performance on low-end devices |

**Missing industry-standard elements to consider**:
- ❌ No pet avatar/sprite (just rotating head icon)
- ❌ No achievement/milestone system
- ❌ No save/load state (localStorage)
- ❌ No tutorial/onboarding

**Recommendation**: The rotating head icon is unique to this haptic project and should be kept as the "pet face". Consider adding subtle expression changes to the SVG based on overall happiness.

### 8.2 Programming Critique

| Concern | Risk Level | Mitigation |
|---------|------------|------------|
| **Audio autoplay blocking** | HIGH | Require user interaction before first sound; show mute button |
| **Web Serial browser support** | MEDIUM | Already handled; Chrome/Edge only |
| **Race conditions in animation state** | MEDIUM | Use `isAnimating` flag; debounce button presses |
| **Memory leaks from emoji particles** | LOW | Remove DOM elements after animation; limit max particles |
| **localStorage quota** | LOW | Only store essential state; implement fallback |

### 8.3 Guaranteed-to-Work Checklist

- [ ] **Audio**: Preload all files; handle `play()` promise rejection
- [ ] **Serial**: Existing implementation is solid; no changes needed
- [ ] **Animations**: Use CSS animations (GPU-accelerated) over JS animations
- [ ] **State**: Initialize all values; validate before applying changes
- [ ] **DOM**: Use `requestAnimationFrame` for smooth updates

---

## 9. Implementation Order

### Phase 1: Core Structure (Est. 2 hours)
1. Restructure HTML layout for new design
2. Add status bar HTML/CSS
3. Restyle buttons with emotion themes
4. Add action text cycling logic

### Phase 2: Animations (Est. 1.5 hours)
1. Implement emoji stream system
2. Add button press animations (ripple, scale)
3. Add text swap animations
4. Add status bar fill animations

### Phase 3: Audio (Est. 30 min)
1. Add audio preloading
2. Hook into `preset_start` event (both sources)
3. Add mute toggle button
4. Test with placeholder sounds

### Phase 4: Game Logic (Est. 1 hour)
1. Implement stat decay system
2. Implement action effects on stats
3. Add action cycling after button press
4. Add localStorage persistence

### Phase 5: Polish (Est. 1 hour)
1. Add critical stat warnings (pulse animation)
2. Add head SVG expression changes based on happiness
3. Test all edge cases
4. Performance optimization

---

## 10. File Structure

```
HapticsCode/
├── haptic_gui.html          # Original control interface (preserved)
├── haptic_pet.html          # NEW: Tamagotchi game interface
├── relaxed.mp3              # Audio: soothing sound (USER MUST PROVIDE)
├── delighted.mp3            # Audio: happy/excited sound (USER MUST PROVIDE)
├── unhappy.mp3              # Audio: sad sound (USER MUST PROVIDE)
├── angry.mp3                # Audio: frustrated sound (USER MUST PROVIDE)
└── docs/
    └── tamagotchi_technical_plan.md  # This document
```

### Audio File Requirements

The user must provide 4 MP3 files in the same directory as `haptic_pet.html`:

| File | Recommended Content | Duration |
|------|--------------------|---------|
| `relaxed.mp3` | Soft chime, gentle hum, nature sounds | 1-3 sec |
| `delighted.mp3` | Happy jingle, celebration sound, sparkle | 1-2 sec |
| `unhappy.mp3` | Sad tone, minor key, soft whimper | 1-2 sec |
| `angry.mp3` | Sharp buzz, growl, alarm-like | 0.5-1 sec |

**Free sound sources:**
- [Freesound.org](https://freesound.org)
- [Pixabay Sound Effects](https://pixabay.com/sound-effects/)
- [Zapsplat](https://www.zapsplat.com)

---

## 11. Testing Checklist

### Functional Tests
- [ ] All 4 buttons trigger correct preset
- [ ] Button text cycles after each press
- [ ] Status bars update correctly per action
- [ ] Status bars decay over time
- [ ] Emoji stream appears on button press
- [ ] Sound plays on GUI-triggered animation
- [ ] Sound plays on robot-triggered animation (physical touch)
- [ ] Animations don't stack/overlap incorrectly
- [ ] Full-size mode still works

### Edge Cases
- [ ] Rapid button mashing doesn't break state
- [ ] Disconnecting mid-animation recovers gracefully
- [ ] Audio blocked by browser shows graceful fallback
- [ ] Stats don't go below 0 or above 100
- [ ] Works on Chrome, Edge (Web Serial requirement)

---

## 12. Future Enhancements (Out of Scope)

- Pet evolution based on care quality
- Mini-games for stat boosts
- Multiple pet personalities
- Multiplayer/social features
- Mobile app wrapper (Capacitor/Cordova)

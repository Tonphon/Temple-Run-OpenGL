# Temple Run OpenGL

A 3D endless runner game inspired by Temple Run, built from scratch using OpenGL, C++, and GLFW. Features procedural map generation, skeletal animation, and dynamic difficulty scaling.

![Game Status](https://img.shields.io/badge/Status-Playable-green)
![OpenGL](https://img.shields.io/badge/OpenGL-3.3-blue)
![C++](https://img.shields.io/badge/C%2B%2B-11-orange)

## Gameplay Features

### Core Mechanics
- **Endless Running**: Automatically move forward at increasing speeds
- **Progressive Difficulty**: Speed multiplier increases from 1.0x to 3.0x over time
- **Three Actions**: Jump over low walls, slide under gates, turn at intersections
- **Health System**: One-hit death system for high stakes gameplay
- **Score System**: Collect coins to increase score
- **Dynamic Speed Display**: Real-time speed multiplier shown in window title

### Controls
| Key | Action |
|-----|--------|
| **Space** | Jump over obstacles |
| **S** | Slide under gates |
| **A** | Turn left (90°) |
| **D** | Turn right (90°) |
| **Mouse** | Lateral movement (strafe) |
| **R** | Restart (when game over) |
| **ESC** | Exit game |

## Technical Features

### Graphics & Rendering
- **Skeletal Animation System**
  - Bone-based character animation with up to 100 bones
  - Support for 4 bone influences per vertex
  - Smooth transitions between running, jumping, and sliding animations
  - Root motion compensation for slide animation

- **Dual Shader System**
  - `anim_model.vs/fs`: Handles skeletal animation for character
  - `static_model.vs/fs`: Renders static geometry with Phong lighting

- **Lighting Model**
  - Ambient lighting (0.3 strength)
  - Diffuse lighting with normal calculations
  - Specular highlights (0.8 strength, shininess 32)
  - Toggle-able lighting for different object types

### Procedural Generation
- **Block-Based Level System**
  - 5x5 unit blocks generated procedurally
  - Four block types: Normal, TurnLeft, TurnRight, TurnStraight
  - Deterministic pattern using modulo arithmetic

- **Generation Rules**
  - Turns every 20 blocks (blocks 20, 40, 60...)
  - Obstacles every 4 blocks starting from block 5 (5, 9, 13, 17...)
  - No obstacles immediately after turns (safety zone)
  - 50 blocks maintained ahead of player
  - Automatic cleanup of blocks 10+ units behind

- **Coin Patterns**
  - 5-block sequences of coins (blocks where index % 10 = 2,3,4,5,6)
  - Coins alternate between left and right sides
  - 50% chance of diagonal transitions at block % 10 = 4
  - Jump trajectories: Coins arc over jump obstacles

### Camera System
- **Third-Person Chase Camera**
  - Position: 6.5 units behind, 3.0 units above player
  - Smooth rotation following with 8x speed multiplier
  - Automatic height adjustment during slides (drops to 2.0)
  - Lerped movement for smooth following (5x lerp speed)
  - Handles 90° turns without jarring transitions

### Collision Detection
- **AABB (Axis-Aligned Bounding Box)**
  - Dynamic player hitbox (changes size when sliding)
  - Smart obstacle bypass system
  - Minimum Translation Vector (MTV) for push-out resolution
  - Separate collision boxes for walls and obstacles

- **Hitbox Dimensions**
  - Standing: 0.7 × 1.8 × 0.5 units
  - Sliding: 0.7 × 1.0 × 0.5 units (40% height reduction)

### Animation System
- **Three Character States**
  - Running: Default forward movement
  - Jumping: Parabolic trajectory with gravity
  - Sliding: Fixed 1.5-second duration with hitbox change

- **Root Motion Fix**
  - Tracks root bone movement during slide animation
  - Cancels unwanted forward displacement
  - Preserves visual animation while maintaining position control

## Project Structure

```
Temple-Run-OpenGL/
├── src/
│   ├── skeletal_animation.cpp    # Main game logic
│   ├── anim_model.vs             # Skeletal animation vertex shader
│   ├── anim_model.fs             # Skeletal animation fragment shader
│   ├── static_model.vs          # Static geometry vertex shader
│   └── static_model.fs          # Static geometry fragment shader
├── resources/
│   ├── objects/
│   │   ├── player/              # Character model and animations
│   │   │   ├── Idle.dae
│   │   │   ├── Running.dae
│   │   │   ├── Jump.dae
│   │   │   └── Running Slide.dae
│   │   ├── coin/
│   │   │   └── Chinese Coin.fbx
│   │   └── map/
│   │       └── free-skybox-basic-sky/
│   │           ├── source/
│   │           │   └── basic_skybox_3d.fbx
│   │           └── textures/
│   │               └── sky_water_landscape.jpg
│   └── textures/
│       ├── wood.jpg           # Floor texture
│       └── green.jpg            # Obstacle texture
└── README.md
```

## Assets & Resources

### 3D Models
- **Guy Dangerous** - Player character model  
  Source: https://sketchfab.com/3d-models/guy-dangerous-c1f40ed35a09478fbe3765080fa63ae0

- **Chinese Coin** - Collectible item  
  Source: https://sketchfab.com/3d-models/chinese-coin-fe672c79593b48aebc1bfedf6432275b

- **FREE - SkyBox Basic Sky** - Environment background  
  Source: https://sketchfab.com/3d-models/free-skybox-basic-sky-b2a4fd1b92c248abaae31975c9ea79e2

### Textures
- **Dark Oak Fine Wood PBR** - Wood textures  
  Source: https://www.sketchuptextureclub.com/textures/architecture/wood/fine-wood/dark-wood/dark-oak-fine-wood-pbr-texture-seamless-22005

### Animations
- Character animations created using **Mixamo** (https://mixamo.com)

## Performance Optimizations

- **Memory Management**
  - Deque-based block storage for O(1) add/remove operations
  - Object pooling for coins (marked collected, not deleted)
  - Maximum 60 blocks in memory at once
  - Automatic cleanup of distant objects

- **Rendering Pipeline**
  1. Opaque geometry (blocks, walls, obstacles)
  2. Animated character (most expensive, done once)
  3. Coins with simple rotation
  4. Skybox rendered last (benefits from early Z-rejection)

- **Resource Sharing**
  - Single cube mesh shared for all box-shaped objects
  - One texture per material type
  - Global model pointers for coins and environment
  - Bone matrix array reused each frame

## Build Requirements

- **OpenGL 3.3** or higher
- **GLFW 3** for window management
- **GLAD** for OpenGL function loading
- **GLM** for mathematics
- **Assimp** for model loading
- **stb_image** for texture loading
- **C++11** compatible compiler

## Technical Specifications

### Physics
- **Forward Speed**: 6.0 units/second (base)
- **Jump Velocity**: 5.0 units/second
- **Gravity**: -12.0 units/second²
- **Slide Duration**: 1.5 seconds
- **Turn Angle**: 90° instant rotation

### World Generation
- **Block Size**: 5×5 units
- **Wall Height**: 2.0 units
- **Generation Distance**: 50 blocks ahead
- **Cleanup Distance**: 10 blocks behind
- **Turn Frequency**: Every 20 blocks
- **Obstacle Frequency**: Every 4 blocks (with exceptions)

### Rendering
- **Window Resolution**: 1280×720 default
- **Field of View**: 50°
- **Max Bones**: 100 per model
- **Bone Influences**: 4 per vertex
- **View Distance**: 600 units

## License

This project is for educational purposes. Original Temple Run game concept belongs to Imangi Studios. All 3D models and textures are credited to their respective creators as listed in the Resources section.

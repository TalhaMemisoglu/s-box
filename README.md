# S&box: Multiplayer FPS with Real-Time Terrain
**S&BOX** is a multiplayer first-person shooter game developed as part of the **CSE396 Computer Engineering Project** course at **Gebze Technical University**.

The game blends the **physical and digital worlds** by using an **Xbox Kinect sensor** to scan a real sandbox and transform it into a dynamic, deformable game map. As players physically reshape the sand or place objects (like gum boxes and marbles), the game terrain and environment update in real time.

This project is a fusion of **image processing, computer vision, depth sensing**, and **multiplayer game design**, offering an immersive hands-on gameplay experience where the physical sandbox directly shapes the virtual battlefield.

# Demo
S&BOX transforms real-world sandbox changes into digital gameplay in real time. Below are some snapshots from key moments in the system:

## Kinect Setup Over Physical Sandbox
![image](https://github.com/user-attachments/assets/e59439f5-dcc4-4209-bc04-29cd6211817e)

## Object Detection
![image](https://github.com/user-attachments/assets/7a849116-ef4c-4aeb-ae94-c6172f317e81)

## Dynamic Terrain Mapping
![Untitled video - Made with Clipchamp (2)](https://github.com/user-attachments/assets/277e6c23-268e-4cbe-9c48-bd83f777a218)

_(Illustrates how the terrain changes in the sandbox are reflected live in the game.)_

## Footage from Game
![WhatsApp Image 2025-06-30 at 12 32 47](https://github.com/user-attachments/assets/7f88b15b-8035-4bd5-9767-c2110901ccf0)

# Features

* _**Real-Time Terrain Transformation**_: Kinect sensor continuously scans a physical sandbox and reflects height changes in-game with dynamic terrain deformation.

* _**Image Processing with OpenCV**_: Physical objects like gum boxes and marbles are detected using real-time RGB image analysis and converted into interactive game elements (e.g., cars and mines).

* _**Multiplayer FPS Gameplay**_: Players compete in a real-time first-person shooter environment with support for combat, movement, looting, and XP scoring.

* _**Cross-Platform LAN Play**_: Fully synchronized multiplayer experience over local network, playable on both PC and Android without cloud streaming.

* _**Game Mechanics**_: Drive vehicles or place traps by physically adding objects into the sandbox — creating a unique bridge between real-world actions and virtual gameplay.

* _**Modular Architecture**_: System divided into Kinect Module (data capture), Game Module (Unreal gameplay), and Server Module (multiplayer backend) for scalability and clarity.

* _**Loot System & Weapon Variety**_: Players can find crates containing rifles, shotguns, grenade launchers, health packs, and armor — all dynamically distributed.

* _**Perspective Switching**_: Switch between first- and third-person view for tactical advantage using a single key.

## Technologies Used

| Component              | Technology / Tool         | Purpose                                                                 |
|------------------------|---------------------------|-------------------------------------------------------------------------|
|  Game Engine         | Unreal Engine 4.27.2       | Core FPS gameplay, multiplayer logic, terrain rendering                 |
|  Image Processing    | OpenCV (C++)               | Detect gum boxes and marbles in real-time from Kinect RGB stream       |
|  Depth Sensing       | Xbox Kinect v2 + SDK       | Capture real-time depth and RGB data from the physical sandbox         |
|  Backend / Sync      | Unreal Dedicated Server    | LAN multiplayer server: synchronization, replication, reconnection     |
|  Packaging           | Android SDK / NDK          | Mobile version build and deployment                                     |
|  Version Control     | Git                        | Team collaboration and version tracking                                 |

## Team / Contributors

| Name                     | Role                                  | Modules Involved                    | GitHub Username              |
|--------------------------|---------------------------------------|-------------------------------------|------------------------------|
| Muhammet Talha Memişoğlu | Team Lead & Developer                 | Game Module                         | [TalhaMemisoglu](https://github.com/TalhaMemisoglu) |
| Musab Kardeş             | Developer                             | Kinect Module - Server Module       | [MKardes](https://github.com/MKardes) |
| Mehmet Emin Baytekin     | Developer                             | Game Module                         | [mehme2](https://github.com/mehme2)              |
| Ahmet Hakan Demir        | Developer                             | Server Module - Kinect Module       | [ahmethakandemir](https://github.com/ahmethakandemir)              |
| Beyzanur Yılmaz          | Developer                             | Kinect Module - Server Module       | [beyzayilmaz9](https://github.com/beyzayilmaz9)              |
| Mert Uğur                | Developer                             | Server Module - Game Module         | [mertugr](https://github.com/mertugr)              |
| Bekir Sadık Altunkaya    | Developer                             | Game Module                         | [bsaltunkaya](https://github.com/bsaltunkaya)              |
| Ahmet Eren Arslan        | Developer                             | Kinect Module - Game Module         | [AhmetErenArslan](https://github.com/AhmetErenArslan)              |


## Conclusion

S&BOX successfully achieved its goals of combining real-time depth sensing, image processing, and multiplayer game development into an immersive and innovative experience. The project was developed as part of the **CSE396 Computer Engineering Project** course at **Gebze Technical University**, and was recognized as **one of the top 2 most successful projects out of 16 groups**.

Our system demonstrates seamless interaction between the physical and digital worlds, allowing players to manipulate a real sandbox and instantly affect the game environment. The integration of Kinect, OpenCV, and Unreal Engine was fully implemented across PC and Android platforms with reliable multiplayer support.

📄 **Detailed technical documentations** can be found in [Project Reports](https://github.com/TalhaMemisoglu/s-box/tree/main/Reports)  
🌐 **Our Web Site:** [sandbox.musabkardes.com.tr](http://sandbox.musabkardes.com.tr)

We are proud to have completed this project with excellence and invite you to explore it further through the resources above.

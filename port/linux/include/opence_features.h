/*
OPENCE_FEATURES.H

Central feature toggle header for OpenCE-Firefight / Forge / Service Rank /
Achievements / Extended Challenges modules merged into OpenCE Ultimate Campaign.

Set any toggle to 0 (or remove its standalone .c file) to disable that feature
cleanly without breaking the rest of the build.
*/

#ifndef OPENCE_FEATURES_H
#define OPENCE_FEATURES_H

#ifndef OPENCE_ENABLE_FIREFIGHT
#define OPENCE_ENABLE_FIREFIGHT      1  /* Wave survival mode, AI dropships, generators */
#endif

#ifndef OPENCE_ENABLE_FORGE
#define OPENCE_ENABLE_FORGE          0  /* 3D Monitor editor, runtime tags, .fmap cache builder */ /* AE hook */
#endif

#ifndef OPENCE_ENABLE_SERVICE_RANK
#define OPENCE_ENABLE_SERVICE_RANK   1  /* MCC Career XP, 11 Tours x 30 Ranks (NO H3 Ranked CSR) */
#endif

#ifndef OPENCE_ENABLE_ACHIEVEMENTS
#define OPENCE_ENABLE_ACHIEVEMENTS   0  /* 39 Achievements, their Gamerscore (1000G) + HUD toast popups.
                                           Off: nothing unlocks; what a profile has unlocked stays saved */
#endif

#ifndef OPENCE_ENABLE_EXT_CHALLENGES
#define OPENCE_ENABLE_EXT_CHALLENGES 0  /* Daily + Weekly XP challenges (Firefight/MP/Campaign), not PLAY's
                                           DAILY CHALLENGE. Off: no progress and no XP; what is saved stays */
#endif

#ifndef OPENCE_ENABLE_NAMEPLATES
#define OPENCE_ENABLE_NAMEPLATES     0  /* MCC nameplates. Off: not offered or drawn; the choice stays saved */
#endif

/* Aliases so both OPENCE_ENABLE_* and OPENCE_FEATURE_* spellings work identically */
#define OPENCE_FEATURE_FIREFIGHT      OPENCE_ENABLE_FIREFIGHT
#define OPENCE_FEATURE_FORGE          OPENCE_ENABLE_FORGE
#define OPENCE_FEATURE_SERVICE_RANK   OPENCE_ENABLE_SERVICE_RANK
#define OPENCE_FEATURE_ACHIEVEMENTS   OPENCE_ENABLE_ACHIEVEMENTS
#define OPENCE_FEATURE_EXT_CHALLENGES OPENCE_ENABLE_EXT_CHALLENGES
#define OPENCE_FEATURE_NAMEPLATES     OPENCE_ENABLE_NAMEPLATES

#endif /* OPENCE_FEATURES_H */

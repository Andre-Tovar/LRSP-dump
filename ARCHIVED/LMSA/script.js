/*
  RouteCraft prototype shell
  This file manages screen navigation, level metadata, and placeholder interactions.
*/

const GAME_NAME = "RouteCraft";
const STAR_FULL = String.fromCharCode(9733);
const STAR_EMPTY = String.fromCharCode(9734);
const CONSOLE_REF_HIDDEN_ATTRIBUTE = "consoleRefHidden";
const VICTORY_SCALE_BUDGET_PERCENT = 66;
const VICTORY_SCALE_LEFT_BUDGET_MULTIPLIER = 2;
const VICTORY_SCORE_ANIMATION_DURATION_MS = 1900;
const ONE_STAR_THRESHOLD_MULTIPLIER = 1.4;
const GAME_CONSOLE_REF_SELECTORS = [
  "#game .top-bar",
  "#game .top-bar > .stat-pill",
  "#game .control-panel",
  "#game .control-panel .panel-section",
  "#game .control-panel .section-heading",
  "#game .control-panel .control-list",
  "#game .control-panel .control-list > *",
  "#game .map-panel",
  "#game .map-panel__header > div",
  "#game .shop-panel",
  "#game .shop-panel .panel-section",
  "#game .shop-panel .section-heading",
  "#game .shop-list",
  "#game .shop-card",
  "#game .shop-card__visual",
  "#game .shop-card__title",
  "#game .shop-card > button",
  "#game .button-column",
  "#game .button-column > *",
].join(", ");

const LEVELS = [
  {
    name: "Level 1 - Foundry Morning",
    difficulty: "Easy",
    benchmark: "$80",
    demand: "42 riders",
    routeLimit: "60 min",
    dutyLimit: "4.0 hrs",
    intersections: "7 intersections",
    edgePickups: "9 road pickups",
    progressStars: 2,
    description:
      "You're building the bus schedule for Logistical Models for Scheduling Architects (LMSA), a manufacturing firm that needs employees collected from roads around the district and brought back to the LMSA manufacturing plant. Be precise! The fate of their mornings are in your hands.",
    levelSelectDescription:
      "LMSA is launching its first bus service, and the morning rush is counting on you. Can you build a pickup plan that proves the system works?",
    featuredRules: [
      "Build a fleet by purchasing buses from the bus rental.",
      "Click on brown \"intersection\" nodes in sequence to form a route.",
      "Click on blue \"bus stop\" nodes to pick up LMSA employees.",
      "Bring passengers back to the LMSA manufacturing plant.",
      "Clear the map without blowing the budget.",
    ],
    gameDescription:
      "Plan one bus at a time, preview route consequences before dispatch, and clear edge demand without buying more fleet than the network actually needs",
    pressureLabel: "Employees On Roads",
    mapTitle: "Foundry District Network",
    mapBadge: "Employees Live On Edges",
    planningInfo:
      "Demand is attached to road segments, so the main choice is how to traverse the graph efficiently, not which stop to click first",
    objectives: [
      "Serve all remaining employee demand and bring every rider back to LMSA",
      "Stay within route time, duty time, and bus capacity constraints",
      "Beat the benchmark with fewer wasted miles and fewer unnecessary buses",
    ],
    stats: {
      selectedBus: "Coach H01",
      routeTime: "38 / 60 min",
      dutyTime: "2.1 / 4.0 hrs",
      load: "14 / 20",
      totalCost: "$3,920 / $80",
      remainingDemand: "18 riders",
    },
    results: {
      subtitle: "Cost, benchmark, and star summary",
      playerCost: "$108",
      optimalCost: "$47",
      gap: "129.8%",
      budget: "$80",
      fleet: "2 buses",
      starsEarned: 1,
      summary:
        "You cleared all demand, but the plan finished above budget and still sits well above the best-known cost for the level",
      highlights: [
        "All riders delivered back to LMSA",
        "4 routes dispatched across 2 buses",
        "Best-known cost for this level is now $47",
      ],
    },
  },
  {
    name: "Level 2 - Swing Shift",
    difficulty: "Medium",
    benchmark: "$325",
    demand: "51 riders",
    routeLimit: "55 min",
    dutyLimit: "4.0 hrs",
    intersections: "8 intersections",
    edgePickups: "11 road pickups",
    progressStars: 3,
    description: "Demand has skyrocketed after LMSA workers learn about the amazingly convenient bus. How will you handle the extra pickups?",
    levelSelectDescription:
      "Demand has skyrocketed after LMSA workers learn about the amazingly convenient bus. How will you handle the extra pickups?",
    featuredRules: [
      "Build a staggered pickup plan that handles the shift change before the tighter route clock catches you",
      "Use each bus for purposeful batches so the final cleanup does not turn into an expensive scramble",
      "Protect the budget by choosing a schedule that stays compact instead of brute-forcing the map",
    ],
    gameDescription:
      "This level introduces a stricter route limit, so the winning move is often a clean second trip instead of one oversized route",
    pressureLabel: "Tighter Route Window",
    mapTitle: "Swing Shift Corridor",
    mapBadge: "55 Minute Route Cap",
    planningInfo:
      "The network is still compact, but the route timer now matters more than raw distance so avoid routes that look efficient but overrun the cap",
    objectives: [
      "Cover staggered demand without breaking the 55-minute route limit",
      "Keep reuse efficient so duty time still supports a second trip",
      "Stay near the $325 benchmark with minimal cleanup mileage",
    ],
    stats: {
      selectedBus: "Shuttle M02",
      routeTime: "41 / 55 min",
      dutyTime: "2.7 / 4.0 hrs",
      load: "5 / 6",
      totalCost: "$273 / $325",
      remainingDemand: "22 riders",
    },
    results: {
      subtitle: "Benchmark check for the swing shift pickup wave",
      playerCost: "$273",
      optimalCost: "$273",
      gap: "0.0%",
      budget: "$325",
      fleet: "2 buses",
      starsEarned: 3,
      summary:
        "This latest two-shuttle schedule clears the swing shift demand under budget and now defines the current optimal replay target",
      highlights: [
        "Two routes cover every pickup without breaking the route cap",
        "Total cost lands at the $273 optimal target",
        "The built-in optimal replay now mirrors your latest submitted Level 2 solution",
      ],
    },
  },
  {
    name: "Level 3 - North Corridor",
    difficulty: "Hard",
    benchmark: "$750",
    demand: "58 riders",
    routeLimit: "65 min",
    dutyLimit: "4.2 hrs",
    intersections: "9 intersections",
    edgePickups: "12 road pickups",
    progressStars: 1,
    description: "Longer roads and separated demand clusters make bus reuse more valuable than simple shortest paths",
    levelSelectDescription:
      "Workers are spreading farther north, and the longer roads are starting to punish sloppy planning. Can you keep the corridor under control?",
    featuredRules: [
      "Cover the long northern roads without sending half-empty buses back to LMSA",
      "Preserve duty time so your strongest vehicle can handle more than one useful trip",
      "Match bus size to the corridor demand so every mile works for the schedule instead of against it",
    ],
    gameDescription:
      "North Corridor expands the graph and asks you to think about loaded miles versus deadheading back toward LMSA",
    pressureLabel: "Long Corridor Demand",
    mapTitle: "North Corridor Network",
    mapBadge: "Duty Time Matters",
    planningInfo:
      "This is the first scenario where duty time is a real strategic resource because long routes can be feasible but still reduce how much reuse you get later",
    objectives: [
      "Serve separated road demand with minimal empty backtracking",
      "Protect duty time so your best bus can still run a second route",
      "Reach the $750 benchmark with a disciplined fleet mix",
    ],
    stats: {
      selectedBus: "Coach H01",
      routeTime: "49 / 65 min",
      dutyTime: "3.4 / 4.0 hrs",
      load: "17 / 20",
      totalCost: "$629 / $750",
      remainingDemand: "19 riders",
    },
    results: {
      subtitle: "Long-corridor fleet performance",
      playerCost: "$629",
      optimalCost: "$629",
      gap: "0.0%",
      budget: "$750",
      fleet: "2 buses",
      starsEarned: 3,
      summary:
        "This two-coach corridor plan clears North Corridor under budget and now defines the current optimal replay target",
      highlights: [
        "Both coach routes stay inside the hard constraints",
        "Total cost lands at the $629 optimal target",
        "The built-in optimal replay now mirrors your latest submitted Level 3 solution",
      ],
    },
  },
  {
    name: "Level 4 - Harbor Loop",
    difficulty: "Extreme",
    benchmark: "$1,300",
    demand: "64 riders",
    routeLimit: "62 min",
    dutyLimit: "4.1 hrs",
    intersections: "10 intersections",
    edgePickups: "13 road pickups",
    progressStars: 2,
    description: "A looped industrial district rewards structured circulation and punishes unnecessary reversals across the bridge",
    levelSelectDescription:
      "The harbor district is getting busier, and every messy bridge crossing wastes valuable time. Can you turn the loop into a clean, efficient schedule?",
    featuredRules: [
      "Sweep the harbor district with deliberate loop routes that keep buses productive while they are away from LMSA",
      "Avoid wasteful bridge recrossings that eat time without clearing meaningful demand",
      "Solve the district with route structure first, then fleet size second",
    ],
    gameDescription:
      "Harbor Loop is about structural planning because once you commit to one side of the graph, the strong move is to finish that work before returning to LMSA",
    pressureLabel: "Bridge Crossing Pressure",
    mapTitle: "Harbor Loop District",
    mapBadge: "Loop Structure Rewards",
    planningInfo:
      "The network encourages circulation instead of short reversals so strong solutions sweep one side of the map cleanly, then unload and reset",
    objectives: [
      "Use loop structure to reduce wasted bridge travel",
      "Keep routes legal while avoiding fragmented pickups",
      "Stay close to the $1,300 benchmark with clean district coverage",
    ],
    stats: {
      selectedBus: "Shuttle M02",
      routeTime: "44 / 62 min",
      dutyTime: "2.9 / 4.0 hrs",
      load: "6 / 6",
      totalCost: "$1,176 / $1,300",
      remainingDemand: "21 riders",
    },
    results: {
      subtitle: "Harbor district benchmark review",
      playerCost: "$1,176",
      optimalCost: "$1,176",
      gap: "0.0%",
      budget: "$1,300",
      fleet: "2 buses",
      starsEarned: 3,
      summary:
        "This two-coach schedule clears Harbor Loop under budget and now defines the current optimal replay target",
      highlights: [
        "Both coach routes stay inside the hard constraints",
        "Total cost lands at the $1,176 optimal target",
        "The built-in optimal replay now mirrors your latest submitted Level 4 solution",
      ],
    },
  },
  {
    name: "Level 5 - Storm Delay",
    levelSelectTitle: "Coming Soon",
    difficulty: "Hard",
    benchmark: "$7,300",
    demand: "69 riders",
    routeLimit: "58 min",
    dutyLimit: "3.9 hrs",
    intersections: "10 intersections",
    edgePickups: "14 road pickups",
    progressStars: 1,
    description: "Compressed time limits and wider demand spacing force hard choices between adding fleet and overworking key buses",
    levelSelectDescription:
      "A fresh dispatch challenge is still getting its final tune-up. Hang tight. It will roll into LMSA soon enough.",
    isComingSoon: true,
    lockedToastMessage: "This level is still getting its finishing touches at LMSA. It will be ready soon enough.",
    featuredRules: [
      "Hold the system together under storm pressure by making every route leg count",
      "Use fleet capacity carefully because a bad purchase or a weak cleanup trip gets expensive fast",
      "Finish the morning surge with a schedule that stays legal even when both route and duty limits are tight",
    ],
    gameDescription:
      "Storm Delay squeezes both route and duty windows, so every extra leg and every redundant purchase is amplified in the cost function",
    pressureLabel: "Compressed Duty Window",
    mapTitle: "Storm Delay Response Map",
    mapBadge: "Tight Route + Duty Caps",
    planningInfo:
      "The hard part here is not finding a feasible plan, it is finding one that does not spiral into expensive cleanup work late in the level",
    objectives: [
      "Respect the shortened route and duty limits",
      "Avoid panic purchases by batching demand intelligently",
      "Finish near the $7,300 benchmark under compressed timing",
    ],
    stats: {
      selectedBus: "Coach H02",
      routeTime: "46 / 58 min",
      dutyTime: "3.1 / 4.0 hrs",
      load: "18 / 20",
      totalCost: "$6,840 / $7,300",
      remainingDemand: "23 riders",
    },
    results: {
      subtitle: "High-pressure scenario benchmark review",
      playerCost: "$7,610",
      optimalCost: "$7,040",
      gap: "8.1%",
      budget: "$7,300",
      fleet: "3 buses",
      starsEarned: 2,
      summary:
        "You handled the hard timing constraints well, but two short cleanup routes increased duty cost enough to miss the best-known solution",
      highlights: [
        "No route or duty violations under the compressed limits",
        "Three buses were enough, but one was underused",
        "The benchmark uses fuller early trips to avoid late cleanup",
      ],
    },
  },
  {
    name: "Level 6 - Full Network",
    levelSelectTitle: "Coming Soon",
    difficulty: "Hard",
    benchmark: "$8,100",
    demand: "82 riders",
    routeLimit: "70 min",
    dutyLimit: "4.5 hrs",
    intersections: "12 intersections",
    edgePickups: "16 road pickups",
    progressStars: 0,
    description: "The full prototype-scale network combines long corridors, dense clusters, and multiple reasonable fleet mixes",
    levelSelectDescription:
      "The biggest LMSA challenge is still being polished behind the scenes. Check back soon and it will be waiting for you.",
    isComingSoon: true,
    lockedToastMessage: "The full-network challenge is almost ready for dispatch. Give it a tiny bit longer.",
    featuredRules: [
      "Coordinate the entire LMSA network so every road pickup fits into one disciplined system",
      "Choose a fleet mix that justifies each purchase across the full day instead of solving one hotspot at a time",
      "Win this map by thinking like a dispatcher balancing the whole operation, not a single route",
    ],
    gameDescription:
      "Full Network is the first map where the whole system matters at once with fleet composition, route structure, duty reuse, and benchmark discipline",
    pressureLabel: "Full System Tradeoffs",
    mapTitle: "Full Network Planning Map",
    mapBadge: "Many Feasible Plans",
    planningInfo:
      "This scenario is meant to feel like a real optimization puzzle because several plans will work, but the best ones eliminate empty movement and keep every purchased bus justified",
    objectives: [
      "Serve all 82 riders across the full graph",
      "Balance bus capacity, route structure, and multi-trip duty usage",
      "Beat the $8,100 benchmark with a complete system-level plan",
    ],
    stats: {
      selectedBus: "Coach H03",
      routeTime: "52 / 70 min",
      dutyTime: "3.5 / 4.0 hrs",
      load: "19 / 20",
      totalCost: "$7,240 / $8,100",
      remainingDemand: "31 riders",
    },
    results: {
      subtitle: "Full-network benchmark comparison",
      playerCost: "$8,260",
      optimalCost: "$7,690",
      gap: "7.4%",
      budget: "$8,100",
      fleet: "3 buses",
      starsEarned: 2,
      summary:
        "The plan solved the entire network with solid structure, but a small amount of empty travel on the outer roads kept it above the benchmark",
      highlights: [
        "All road demand cleared across the largest scenario",
        "Three buses handled the work without violating duty caps",
        "The benchmark trims cost by consolidating one outer-loop route",
      ],
    },
  },
];

const DEFAULT_LEVEL = LEVELS[0].name;
const FOUNDRY_FALLBACK_NODE_POSITIONS = {
  1: { top: 46.4, left: 14.3 },
  2: { top: 26.2, left: 45.3 },
  3: { top: 18.3, left: 65.2 },
  4: { top: 17.7, left: 80.9 },
  5: { top: 66.4, left: 20 },
  6: { top: 77.1, left: 25.6 },
  7: { top: 75.2, left: 47.9 },
  8: { top: 39.9, left: 73.1 },
  9: { top: 48.5, left: 74.2 },
  10: { top: 48.7, left: 85 },
  11: { top: 85.2, left: 26.2 },
  12: { top: 67.8, left: 66.3 },
  13: { top: 66.3, left: 76.7 },
  14: { top: 85.7, left: 57.2 },
  15: { top: 96.3, left: 28.9 },
  16: { top: 28.3, left: 79.6 },
  18: { top: 0.2, left: 30.8 },
  19: { top: 22.1, left: 19.1 },
  20: { top: 0.2, left: 47.9 },
  23: { top: 0.2, left: 74.9 },
  24: { top: 6.7, left: 97.8 },
  27: { top: 52.8, left: 99.1 },
  37: { top: 60.4, left: 0.9 },
  38: { top: 52.0, left: 0.2 },
};
const FOUNDRY_FALLBACK_ROADS = [
  { from: "3", to: "4" },
  { from: "4", to: "16" },
  { from: "8", to: "16" },
  { from: "3", to: "8" },
  { from: "2", to: "3" },
  { from: "1", to: "2" },
  { from: "2", to: "7" },
  { from: "1", to: "5" },
  { from: "5", to: "7" },
  { from: "8", to: "9" },
  { from: "7", to: "9" },
  { from: "9", to: "10" },
  { from: "10", to: "16" },
  { from: "4", to: "24" },
  { from: "10", to: "27" },
  { from: "1", to: "38" },
  { from: "5", to: "37" },
];
const FOUNDRY_FALLBACK_CAMPUS_ASSETS = {
  factory: { left: 15.5, top: 27.4, rotation: -0.9, scale: 2.4 },
};
const BUS_SHOP_CONFIG = {
  lift: {
    name: "Personal Lift",
    shortCode: "PL",
    badge: "L",
    purchaseCost: 30,
    capacity: 2,
    routeLimitMinutes: 20,
    dutyLimitMinutes: 60,
  },
  shuttle: {
    name: "Mini Shuttle",
    shortCode: "MS",
    badge: "S",
    purchaseCost: 60,
    capacity: 6,
    routeLimitMinutes: 40,
    dutyLimitMinutes: 60,
  },
  coach: {
    name: "Coach",
    shortCode: "C",
    badge: "C",
    purchaseCost: 120,
    capacity: 20,
    routeLimitMinutes: 60,
    dutyLimitMinutes: 60,
  },
};
const FOUNDRY_FALLBACK_CONFETTI_EMITTER_POSITIONS = [
  { left: 14.7, top: 6.5 },
  { left: 18.7, top: 9.1 },
];
const FOUNDRY_FACTORY_START_LABEL = "factory";
const FOUNDRY_FACTORY_START_CONNECTIONS = ["1", "2"];
const FOUNDRY_FACTORY_ROUNDABOUT_LAYOUT = {
  width: 74,
  height: 38,
  anchorRatio: 0.63,
  curveSamples: 6,
};
const FOUNDRY_ROUTE_MINUTES_PER_EDGE = 3;
const FOUNDRY_PICKUP_NODE_MINUTES = 1;
const FOUNDRY_OPPORTUNITY_COST_PER_HOUR = 30;
const FOUNDRY_REPLAY_MS_PER_MINUTE = 1000;
const FOUNDRY_ROUTE_ANIMATION_PIXELS_PER_SECOND = 170;
const FOUNDRY_ROUTE_INTERSECTION_PAUSE_MS = 140;
const FOUNDRY_ROUTE_PICKUP_PAUSE_MS = 980;
const FOUNDRY_ROUTE_DEPOT_PAUSE_MS = 320;
const GUIDE_REPLAY_LEVEL_NAME = "Level 1 - Foundry Morning";
const GUIDE_REPLAY_MS_PER_MINUTE = 240;
const GUIDE_REPLAY_PICKUP_PAUSE_MS = 720;
const GUIDE_REPLAY_DEPOT_PAUSE_MS = 420;
const GUIDE_REPLAY_LOOP_GAP_MS = 520;
const FOUNDRY_LIVE_RUNNER_ID = "foundry-live-runner";
const VICTORY_OVERLAY_FADE_DURATION_MS = 220;
const FOUNDRY_CONFETTI_EMITTER_STORAGE_KEY = "routecraft-foundry-confetti-emitters";
const FOUNDRY_SCHEDULE_REPLAY_STORAGE_KEY = "routecraft-foundry-schedule-replay-v1";
const FOUNDRY_SUBMITTED_SCHEDULE_REPLAY_STORAGE_KEY = "routecraft-foundry-submitted-schedule-replay-v1";
const FOUNDRY_LEGACY_EDIT_LAYOUT_STORAGE_KEY = "routecraft-foundry-edit-layout-v1";
const FOUNDRY_EDIT_LAYOUT_STORAGE_KEY_PREFIX = "routecraft-foundry-edit-layout-v2:";
const FOUNDRY_PREFER_CODE_LAYOUTS_OVER_LOCAL_STORAGE = true;
const FOUNDRY_CUSTOM_NODE_FIRST_LABEL = 41;
const FOUNDRY_DEFAULT_BUILDER_STOP_SIGN_SCALE = 1.8;
const FOUNDRY_BUILDER_DEMAND_PAIR_MAX_DISTANCE = 10;
const FOUNDRY_BUILDER_BLUE_NODE_ROPE_RADIUS_PX = 36;
const FOUNDRY_LEVEL_STOP_BASE_DEMANDS = {
  "Level 1 - Foundry Morning": {
    "pickup-1": 1,
    "pickup-3": 1,
  },
};
const FOUNDRY_LEVEL_PICKUP_NODE_CONFIGS = {
  "Level 1 - Foundry Morning": {
    "pickup-1": { anchorLeft: 41, anchorTop: 44.6 },
    "pickup-3": { anchorLeft: 28.3, anchorTop: 63.3 },
  },
};
const FOUNDRY_OPTIMAL_SCHEDULE_REPLAY_SNAPSHOTS = {
  "Level 1 - Foundry Morning": {
    version: 1,
    levelName: "Level 1 - Foundry Morning",
    capturedAt: 1776570933612,
    buses: [
      {
        id: "lift-01",
        sequence: 1,
        type: "lift",
        purchaseCost: 30,
        capacity: 2,
        totalCost: 47,
        completedRouteHistory: [
          {
            routeNodeLabels: ["factory", "2", "pickup-1", "7", "pickup-3", "5", "1", "factory"],
            startMinute: 0,
            durationMinutes: 17,
            pickupPlan: {
              2: 1,
              4: 1,
            },
            stopCounts: {
              "pickup-1": 1,
              "pickup-3": 1,
            },
          },
        ],
      },
    ],
  },
  "Level 2 - Swing Shift": {
    version: 1,
    levelName: "Level 2 - Swing Shift",
    capturedAt: 1776572546855,
    buses: [
      {
        id: "shuttle-01",
        sequence: 1,
        type: "shuttle",
        purchaseCost: 60,
        capacity: 6,
        totalCost: 135,
        completedRouteHistory: [
          {
            routeNodeLabels: ["factory", "2", "59", "51", "54", "1", "factory"],
            startMinute: 0,
            durationMinutes: 14,
            pickupPlan: {
              2: 3,
              4: 3,
            },
            stopCounts: {
              54: 3,
              59: 3,
            },
          },
          {
            routeNodeLabels: ["factory", "2", "41", "7", "53", "51", "2", "factory"],
            startMinute: 14,
            durationMinutes: 19,
            pickupPlan: {
              4: 2,
            },
            stopCounts: {
              53: 2,
            },
          },
        ],
      },
      {
        id: "shuttle-02",
        sequence: 2,
        type: "shuttle",
        purchaseCost: 60,
        capacity: 6,
        totalCost: 137.5,
        completedRouteHistory: [
          {
            routeNodeLabels: ["factory", "2", "47", "57", "4", "10", "58", "9", "43", "55", "41", "56", "2", "factory"],
            startMinute: 0,
            durationMinutes: 31,
            pickupPlan: {
              3: 1,
              6: 1,
              9: 1,
              11: 2,
            },
            stopCounts: {
              55: 1,
              56: 2,
              57: 1,
              58: 1,
            },
          },
        ],
      },
    ],
  },
  "Level 3 - North Corridor": {
    version: 1,
    levelName: "Level 3 - North Corridor",
    capturedAt: 1776633328823,
    buses: [
      {
        id: "coach-01",
        sequence: 1,
        type: "coach",
        purchaseCost: 120,
        capacity: 20,
        totalCost: 328.5,
        completedRouteHistory: [
          {
            routeNodeLabels: ["factory", "2", "47", "63", "64", "43", "62", "61", "4", "57", "47", "2", "factory"],
            startMinute: 0,
            durationMinutes: 28,
            pickupPlan: {
              3: 3,
              4: 2,
              6: 2,
              9: 2,
            },
            stopCounts: {
              57: 2,
              62: 2,
              63: 3,
              64: 2,
            },
          },
          {
            routeNodeLabels: ["factory", "2", "41", "9", "58", "10", "71", "70", "7", "47", "2", "factory"],
            startMinute: 28,
            durationMinutes: 27,
            pickupPlan: {
              4: 1,
              6: 1,
              7: 1,
            },
            stopCounts: {
              58: 1,
              70: 1,
              71: 1,
            },
          },
        ],
      },
      {
        id: "coach-02",
        sequence: 2,
        type: "coach",
        purchaseCost: 120,
        capacity: 20,
        totalCost: 300,
        completedRouteHistory: [
          {
            routeNodeLabels: ["factory", "2", "59", "60", "51", "54", "1", "factory"],
            startMinute: 0,
            durationMinutes: 15,
            pickupPlan: {
              2: 4,
              3: 3,
              5: 4,
            },
            stopCounts: {
              54: 4,
              59: 4,
              60: 3,
            },
          },
          {
            routeNodeLabels: ["factory", "1", "5", "66", "69", "7", "68", "65", "51", "2", "factory"],
            startMinute: 15,
            durationMinutes: 24,
            pickupPlan: {
              3: 2,
              4: 1,
              6: 2,
            },
            stopCounts: {
              66: 2,
              68: 2,
              69: 1,
            },
          },
        ],
      },
    ],
  },
  "Level 4 - Harbor Loop": {
    version: 1,
    levelName: "Level 4 - Harbor Loop",
    capturedAt: 1776632361834,
    buses: [
      {
        id: "coach-02",
        sequence: 2,
        type: "coach",
        purchaseCost: 120,
        capacity: 20,
        totalCost: 576,
        completedRouteHistory: [
          {
            routeNodeLabels: ["factory", "2", "68", "70", "69", "47", "63", "64", "43", "89", "9", "58", "10", "61", "62", "43", "4", "57", "47", "69", "70", "68", "2", "factory"],
            startMinute: 0,
            durationMinutes: 57,
            pickupPlan: {
              6: 3,
              7: 4,
              9: 4,
              11: 3,
              14: 1,
              17: 1,
            },
            stopCounts: {
              57: 1,
              58: 3,
              62: 1,
              63: 3,
              64: 4,
              89: 4,
            },
          },
        ],
      },
      {
        id: "coach-01",
        sequence: 1,
        type: "coach",
        purchaseCost: 120,
        capacity: 20,
        totalCost: 600,
        completedRouteHistory: [
          {
            routeNodeLabels: ["factory", "2", "87", "88", "41", "90", "72", "84", "77", "86", "85", "78", "91", "5", "65", "76", "93", "79", "65", "92", "51", "1", "factory"],
            startMinute: 0,
            durationMinutes: 48,
            pickupPlan: {
              2: 2,
              3: 3,
              5: 5,
              7: 4,
              9: 1,
              10: 2,
              12: 1,
              16: 1,
              19: 1,
            },
            stopCounts: {
              84: 4,
              85: 2,
              86: 1,
              87: 2,
              88: 3,
              90: 5,
              91: 1,
              92: 1,
              93: 1,
            },
          },
        ],
      },
    ],
  },
};
const FOUNDRY_SAVED_EDIT_LAYOUT_FALLBACKS = {
  "Level 2 - Swing Shift": {
    version: 1,
    levelName: "Level 2 - Swing Shift",
    capturedAt: 1776571347980,
    nodes: [
      { label: "41", variant: "brown", left: 57.1, top: 54.1, demand: 0 },
      { label: "42", variant: "brown", left: 9.2, top: 91, demand: 0 },
      { label: "43", variant: "brown", left: 70.1, top: 35.8, demand: 0 },
      { label: "46", variant: "brown", left: 9.8, top: 88.8, demand: 0 },
      { label: "47", variant: "brown", left: 62.7, top: 12.7, demand: 0 },
      { label: "51", variant: "brown", left: 32.2, top: 48, demand: 0 },
      { label: "53", variant: "blue", left: 40.1, top: 61.4, demand: 2 },
      { label: "54", variant: "blue", left: 24.4, top: 47.3, demand: 3 },
      { label: "55", variant: "blue", left: 64, top: 44.3, demand: 1 },
      { label: "56", variant: "blue", left: 51.5, top: 40.5, demand: 2 },
      { label: "57", variant: "blue", left: 71.8, top: 15.1, demand: 1 },
      { label: "58", variant: "blue", left: 79.4, top: 48.6, demand: 1 },
      { label: "59", variant: "blue", left: 38.6, top: 37.5, demand: 3 },
    ],
    assets: [],
    roads: [
      { from: "1", to: "2" },
      { from: "4", to: "24" },
      { from: "10", to: "27" },
      { from: "1", to: "38" },
      { from: "5", to: "37" },
      { from: "7", to: "41" },
      { from: "9", to: "41" },
      { from: "2", to: "47" },
      { from: "43", to: "47" },
      { from: "9", to: "43" },
      { from: "4", to: "43" },
      { from: "4", to: "10" },
      { from: "2", to: "43" },
      { from: "5", to: "7" },
      { from: "41", to: "51" },
      { from: "1", to: "5" },
      { from: "51", to: "53" },
      { from: "7", to: "53" },
      { from: "1", to: "54" },
      { from: "51", to: "54" },
      { from: "43", to: "55" },
      { from: "41", to: "55" },
      { from: "2", to: "56" },
      { from: "41", to: "56" },
      { from: "47", to: "57" },
      { from: "4", to: "57" },
      { from: "9", to: "58" },
      { from: "10", to: "58" },
      { from: "51", to: "59" },
      { from: "2", to: "59" },
    ],
    legacyNodeOverrides: [],
    legacyAssetOverrides: [],
  },
  "Level 3 - North Corridor": {
    version: 1,
    levelName: "Level 3 - North Corridor",
    capturedAt: 1776570811766,
    nodes: [
      { label: "41", variant: "brown", left: 58.9, top: 51.2, demand: 0 },
      { label: "42", variant: "brown", left: 9.2, top: 91, demand: 0 },
      { label: "43", variant: "brown", left: 70.1, top: 35.8, demand: 0 },
      { label: "46", variant: "brown", left: 9.8, top: 88.8, demand: 0 },
      { label: "47", variant: "brown", left: 62.7, top: 12.7, demand: 0 },
      { label: "51", variant: "brown", left: 31.9, top: 48, demand: 0 },
      { label: "54", variant: "blue", left: 24.4, top: 47.3, demand: 4 },
      { label: "57", variant: "blue", left: 71.8, top: 15.1, demand: 2 },
      { label: "58", variant: "blue", left: 79.4, top: 48.6, demand: 1 },
      { label: "59", variant: "blue", left: 40.2, top: 34.7, demand: 4 },
      { label: "60", variant: "blue", left: 36.3, top: 41.3, demand: 3 },
      { label: "61", variant: "brown", left: 83.4, top: 34.2, demand: 0 },
      { label: "62", variant: "blue", left: 76.9, top: 35, demand: 2 },
      { label: "63", variant: "blue", left: 65.8, top: 22.1, demand: 3 },
      { label: "64", variant: "blue", left: 68.2, top: 29.9, demand: 2 },
      { label: "65", variant: "brown", left: 38.1, top: 62.7, demand: 0 },
      { label: "66", variant: "blue", left: 31.1, top: 70.3, demand: 2 },
      { label: "67", variant: "brown", left: 10.1, top: 90.7, demand: 0 },
      { label: "68", variant: "blue", left: 42.9, top: 68.4, demand: 2 },
      { label: "69", variant: "blue", left: 37.9, top: 72.4, demand: 1 },
      { label: "70", variant: "blue", left: 60.8, top: 66.2, demand: 1 },
      { label: "71", variant: "blue", left: 70, top: 59.6, demand: 1 },
    ],
    assets: [],
    roads: [
      { from: "1", to: "2" },
      { from: "4", to: "24" },
      { from: "10", to: "27" },
      { from: "1", to: "38" },
      { from: "5", to: "37" },
      { from: "9", to: "41" },
      { from: "2", to: "47" },
      { from: "4", to: "43" },
      { from: "1", to: "5" },
      { from: "1", to: "54" },
      { from: "51", to: "54" },
      { from: "47", to: "57" },
      { from: "4", to: "57" },
      { from: "9", to: "58" },
      { from: "10", to: "58" },
      { from: "2", to: "59" },
      { from: "59", to: "60" },
      { from: "51", to: "60" },
      { from: "10", to: "61" },
      { from: "4", to: "61" },
      { from: "41", to: "43" },
      { from: "43", to: "62" },
      { from: "61", to: "62" },
      { from: "5", to: "43" },
      { from: "47", to: "63" },
      { from: "63", to: "64" },
      { from: "43", to: "64" },
      { from: "9", to: "43" },
      { from: "5", to: "65" },
      { from: "51", to: "65" },
      { from: "41", to: "65" },
      { from: "2", to: "41" },
      { from: "5", to: "66" },
      { from: "65", to: "68" },
      { from: "7", to: "68" },
      { from: "66", to: "69" },
      { from: "7", to: "69" },
      { from: "7", to: "47" },
      { from: "7", to: "70" },
      { from: "70", to: "71" },
      { from: "10", to: "71" },
    ],
    legacyNodeOverrides: [
      { label: "7", left: 47.9, top: 75.2, removed: false },
    ],
    legacyAssetOverrides: [],
  },
  "Level 4 - Harbor Loop": {
    version: 1,
    levelName: "Level 4 - Harbor Loop",
    capturedAt: 1776631290042,
    nodes: [
      { label: "41", variant: "brown", left: 48.4, top: 42, demand: 0 },
      { label: "42", variant: "brown", left: 9.2, top: 91, demand: 0 },
      { label: "43", variant: "brown", left: 70.1, top: 35.8, demand: 0 },
      { label: "46", variant: "brown", left: 9.8, top: 88.8, demand: 0 },
      { label: "47", variant: "brown", left: 62.7, top: 12.7, demand: 0 },
      { label: "51", variant: "brown", left: 37.4, top: 40, demand: 0 },
      { label: "57", variant: "blue", left: 71.8, top: 15.1, demand: 1 },
      { label: "58", variant: "blue", left: 79.4, top: 48.6, demand: 3 },
      { label: "61", variant: "brown", left: 83.4, top: 34.2, demand: 0 },
      { label: "62", variant: "blue", left: 76.9, top: 35, demand: 1 },
      { label: "63", variant: "blue", left: 65.8, top: 22.1, demand: 3 },
      { label: "64", variant: "blue", left: 68.2, top: 29.9, demand: 4 },
      { label: "65", variant: "brown", left: 28.2, top: 55.7, demand: 0 },
      { label: "67", variant: "brown", left: 10.1, top: 90.7, demand: 0 },
      { label: "68", variant: "brown", left: 47.6, top: 19.8, demand: 0 },
      { label: "69", variant: "brown", left: 57.6, top: 12.2, demand: 0 },
      { label: "70", variant: "brown", left: 52.1, top: 14.8, demand: 0 },
      { label: "71", variant: "brown", left: 9.7, top: 90.8, demand: 0 },
      { label: "72", variant: "brown", left: 52.3, top: 55.9, demand: 0 },
      { label: "73", variant: "brown", left: 65.1, top: 32.1, demand: 0 },
      { label: "74", variant: "brown", left: 59.1, top: 31.6, demand: 0 },
      { label: "75", variant: "brown", left: 53.7, top: 35.6, demand: 0 },
      { label: "76", variant: "brown", left: 33.9, top: 63, demand: 0 },
      { label: "77", variant: "brown", left: 47.5, top: 67.2, demand: 0 },
      { label: "78", variant: "brown", left: 31.1, top: 73.6, demand: 0 },
      { label: "79", variant: "brown", left: 41.2, top: 50, demand: 0 },
      { label: "81", variant: "brown", left: 69.4, top: 46.9, demand: 0 },
      { label: "82", variant: "brown", left: 63.5, top: 46.1, demand: 0 },
      { label: "83", variant: "brown", left: 57.4, top: 49.8, demand: 0 },
      { label: "84", variant: "blue", left: 49.8, top: 62.2, demand: 4 },
      { label: "85", variant: "blue", left: 36.3, top: 71.8, demand: 2 },
      { label: "86", variant: "blue", left: 41.6, top: 69.8, demand: 1 },
      { label: "87", variant: "blue", left: 45.7, top: 32.3, demand: 2 },
      { label: "88", variant: "blue", left: 47.2, top: 37.8, demand: 3 },
      { label: "89", variant: "blue", left: 72.3, top: 42.7, demand: 4 },
      { label: "90", variant: "blue", left: 50.5, top: 49.4, demand: 5 },
      { label: "91", variant: "blue", left: 25.6, top: 71.1, demand: 1 },
      { label: "92", variant: "blue", left: 32.9, top: 47.8, demand: 1 },
      { label: "93", variant: "blue", left: 36.8, top: 57.9, demand: 1 },
    ],
    assets: [],
    roads: [
      { from: "1", to: "2" },
      { from: "4", to: "24" },
      { from: "10", to: "27" },
      { from: "1", to: "38" },
      { from: "5", to: "37" },
      { from: "4", to: "43" },
      { from: "47", to: "57" },
      { from: "4", to: "57" },
      { from: "9", to: "58" },
      { from: "10", to: "58" },
      { from: "10", to: "61" },
      { from: "4", to: "61" },
      { from: "43", to: "62" },
      { from: "61", to: "62" },
      { from: "47", to: "63" },
      { from: "63", to: "64" },
      { from: "43", to: "64" },
      { from: "5", to: "65" },
      { from: "1", to: "51" },
      { from: "2", to: "51" },
      { from: "2", to: "68" },
      { from: "47", to: "69" },
      { from: "68", to: "70" },
      { from: "69", to: "70" },
      { from: "41", to: "75" },
      { from: "74", to: "75" },
      { from: "73", to: "74" },
      { from: "43", to: "73" },
      { from: "76", to: "77" },
      { from: "72", to: "79" },
      { from: "65", to: "79" },
      { from: "41", to: "79" },
      { from: "76", to: "78" },
      { from: "51", to: "79" },
      { from: "1", to: "5" },
      { from: "72", to: "83" },
      { from: "82", to: "83" },
      { from: "81", to: "82" },
      { from: "9", to: "81" },
      { from: "77", to: "84" },
      { from: "72", to: "84" },
      { from: "78", to: "85" },
      { from: "85", to: "86" },
      { from: "77", to: "86" },
      { from: "41", to: "88" },
      { from: "87", to: "88" },
      { from: "2", to: "87" },
      { from: "9", to: "89" },
      { from: "43", to: "89" },
      { from: "41", to: "90" },
      { from: "72", to: "90" },
      { from: "41", to: "51" },
      { from: "77", to: "79" },
      { from: "65", to: "76" },
      { from: "78", to: "91" },
      { from: "5", to: "91" },
      { from: "65", to: "92" },
      { from: "51", to: "92" },
      { from: "76", to: "93" },
      { from: "79", to: "93" },
    ],
    legacyNodeOverrides: [
      { label: "7", left: 33.8, top: 72, removed: false },
      { label: "5", left: 20.2, top: 68.8, removed: false },
      { label: "2", left: 44.2, top: 26.9, removed: false },
    ],
    legacyAssetOverrides: [],
  },
  "Level 5 - Storm Delay": {
    version: 1,
    levelName: "Level 5 - Storm Delay",
    capturedAt: 1776567750085,
    nodes: [
      { label: "41", variant: "brown", left: 57.1, top: 54.1, demand: 0 },
      { label: "42", variant: "brown", left: 9.2, top: 91, demand: 0 },
      { label: "43", variant: "brown", left: 70.1, top: 35.8, demand: 0 },
      { label: "46", variant: "brown", left: 9.8, top: 88.8, demand: 0 },
      { label: "47", variant: "brown", left: 62.7, top: 12.7, demand: 0 },
      { label: "51", variant: "brown", left: 32.2, top: 48, demand: 0 },
      { label: "53", variant: "blue", left: 34.6, top: 59.7, demand: 2 },
      { label: "54", variant: "blue", left: 24.4, top: 47.3, demand: 3 },
      { label: "55", variant: "blue", left: 64, top: 44.3, demand: 1 },
      { label: "56", variant: "blue", left: 51.5, top: 40.5, demand: 2 },
      { label: "57", variant: "blue", left: 71.8, top: 15.1, demand: 1 },
      { label: "58", variant: "blue", left: 79.4, top: 48.6, demand: 1 },
      { label: "59", variant: "blue", left: 38.6, top: 37.5, demand: 3 },
    ],
    assets: [],
    roads: [
      { from: "1", to: "2" },
      { from: "4", to: "24" },
      { from: "10", to: "27" },
      { from: "1", to: "38" },
      { from: "5", to: "37" },
      { from: "7", to: "41" },
      { from: "9", to: "41" },
      { from: "2", to: "47" },
      { from: "43", to: "47" },
      { from: "9", to: "43" },
      { from: "4", to: "43" },
      { from: "4", to: "10" },
      { from: "2", to: "43" },
      { from: "5", to: "7" },
      { from: "41", to: "51" },
      { from: "1", to: "5" },
      { from: "51", to: "53" },
      { from: "7", to: "53" },
      { from: "1", to: "54" },
      { from: "51", to: "54" },
      { from: "43", to: "55" },
      { from: "41", to: "55" },
      { from: "2", to: "56" },
      { from: "41", to: "56" },
      { from: "47", to: "57" },
      { from: "4", to: "57" },
      { from: "9", to: "58" },
      { from: "10", to: "58" },
      { from: "51", to: "59" },
      { from: "2", to: "59" },
    ],
    legacyNodeOverrides: [],
    legacyAssetOverrides: [],
  },
  "Level 6 - Full Network": {
    version: 1,
    levelName: "Level 6 - Full Network",
    capturedAt: 1776567750085,
    nodes: [
      { label: "41", variant: "brown", left: 57.1, top: 54.1, demand: 0 },
      { label: "42", variant: "brown", left: 9.2, top: 91, demand: 0 },
      { label: "43", variant: "brown", left: 70.1, top: 35.8, demand: 0 },
      { label: "46", variant: "brown", left: 9.8, top: 88.8, demand: 0 },
      { label: "47", variant: "brown", left: 62.7, top: 12.7, demand: 0 },
      { label: "51", variant: "brown", left: 32.2, top: 48, demand: 0 },
      { label: "53", variant: "blue", left: 34.6, top: 59.7, demand: 2 },
      { label: "54", variant: "blue", left: 24.4, top: 47.3, demand: 3 },
      { label: "55", variant: "blue", left: 64, top: 44.3, demand: 1 },
      { label: "56", variant: "blue", left: 51.5, top: 40.5, demand: 2 },
      { label: "57", variant: "blue", left: 71.8, top: 15.1, demand: 1 },
      { label: "58", variant: "blue", left: 79.4, top: 48.6, demand: 1 },
      { label: "59", variant: "blue", left: 38.6, top: 37.5, demand: 3 },
    ],
    assets: [],
    roads: [
      { from: "1", to: "2" },
      { from: "4", to: "24" },
      { from: "10", to: "27" },
      { from: "1", to: "38" },
      { from: "5", to: "37" },
      { from: "7", to: "41" },
      { from: "9", to: "41" },
      { from: "2", to: "47" },
      { from: "43", to: "47" },
      { from: "9", to: "43" },
      { from: "4", to: "43" },
      { from: "4", to: "10" },
      { from: "2", to: "43" },
      { from: "5", to: "7" },
      { from: "41", to: "51" },
      { from: "1", to: "5" },
      { from: "51", to: "53" },
      { from: "7", to: "53" },
      { from: "1", to: "54" },
      { from: "51", to: "54" },
      { from: "43", to: "55" },
      { from: "41", to: "55" },
      { from: "2", to: "56" },
      { from: "41", to: "56" },
      { from: "47", to: "57" },
      { from: "4", to: "57" },
      { from: "9", to: "58" },
      { from: "10", to: "58" },
      { from: "51", to: "59" },
      { from: "2", to: "59" },
    ],
    legacyNodeOverrides: [],
    legacyAssetOverrides: [],
  },
};

const LMSAApp = (() => {
  const state = {
    currentScreen: "landing",
    currentLevel: DEFAULT_LEVEL,
    foundryRoads: [],
    purchasedFleet: [],
    selectedFleetBusId: "",
    selectedFoundryNodeLabel: "",
    suppressNextFoundryRoadClick: false,
    foundryAnimatingBusId: "",
    foundryAnimationFrame: 0,
    foundrySolutionReplayFrame: 0,
    guideReplayFrame: 0,
    guideReplayStartTime: 0,
    guideReplayCycleIndex: -1,
    guideReplayTimeline: null,
    foundrySolutionReplayResolve: null,
    foundryStopDemand: { ...getFoundryLevelStopBaseDemands(DEFAULT_LEVEL) },
    foundryReplayStopDemand: null,
    foundryReplayAppliedPickupKeys: new Set(),
    foundryPickupLayouts: {},
    foundryLoadedEditLayout: null,
    foundryCustomNodes: [],
    foundryCustomAssets: [],
    foundryLegacyNodeOverrides: [],
    foundryLegacyAssetOverrides: [],
    selectedFoundryBuilderAssetId: "",
    debugForceSubmitReady: false,
    victoryOverlayTimeoutId: 0,
    victoryScoreAnimationFrame: 0,
    isFoundrySolutionReplayActive: false,
    isVictoryOverlayVisible: false,
    toastTimeoutId: 0,
    isEditMode: false,
  };

  const dom = {};

  function starBand(filledCount) {
    return `${STAR_FULL.repeat(filledCount)}${STAR_EMPTY.repeat(3 - filledCount)}`;
  }

  function cacheDom() {
    dom.screens = Array.from(document.querySelectorAll(".screen"));
    dom.gameScreen = document.getElementById("game");
    dom.resultsSubtitle = document.getElementById("resultsSubtitle");
    dom.headerCurrentPage = document.getElementById("headerCurrentPage");
    dom.headerCurrentSubpage = document.getElementById("headerCurrentSubpage");
    dom.levelGrid = document.getElementById("levelGrid");
    dom.featuredLevelTitle = document.getElementById("featuredLevelTitle");
    dom.featuredLevelDescription = document.getElementById("featuredLevelDescription");
    dom.featuredBudget = document.getElementById("featuredBudget");
    dom.featuredDifficulty = document.getElementById("featuredDifficulty");
    dom.featuredRuleList = document.getElementById("featuredRuleList");
    dom.guideReplayStage = document.getElementById("guideReplayStage");
    dom.guideReplayRoadLayer = document.getElementById("guideReplayRoadLayer");
    dom.guideReplayNodeLayer = document.getElementById("guideReplayNodeLayer");
    dom.guideReplayRunner = document.getElementById("guideReplayRunner");
    dom.guideReplayRunnerBody = document.getElementById("guideReplayRunnerBody");
    dom.statSelectedBus = document.getElementById("statSelectedBus");
    dom.statSelectedBusDetail = document.getElementById("statSelectedBusDetail");
    dom.statRouteTime = document.getElementById("statRouteTime");
    dom.statRouteTimeDetail = document.getElementById("statRouteTimeDetail");
    dom.statDutyTime = document.getElementById("statDutyTime");
    dom.statDutyTimeDetail = document.getElementById("statDutyTimeDetail");
    dom.statLoad = document.getElementById("statLoad");
    dom.statLoadDetail = document.getElementById("statLoadDetail");
    dom.statTotalCost = document.getElementById("statTotalCost");
    dom.statTotalCostDetail = document.getElementById("statTotalCostDetail");
    dom.statRemainingDemand = document.getElementById("statRemainingDemand");
    dom.mapPanelTitle = document.getElementById("mapPanelTitle");
    dom.resultPlayerCost = document.getElementById("resultPlayerCost");
    dom.resultOptimalCost = document.getElementById("resultOptimalCost");
    dom.resultGap = document.getElementById("resultGap");
    dom.resultBudget = document.getElementById("resultBudget");
    dom.resultFleet = document.getElementById("resultFleet");
    dom.resultStars = document.getElementById("resultStars");
    dom.resultsSummaryText = document.getElementById("resultsSummaryText");
    dom.resultsHighlights = document.getElementById("resultsHighlights");
    dom.foundryMapStage = document.querySelector(".map-stage--foundry");
    dom.foundryZoneLayer = document.getElementById("foundryZoneLayer");
    dom.foundryRoadLayer = document.getElementById("foundryRoadLayer");
    dom.foundryAnimationLayer = document.getElementById("foundryAnimationLayer");
    dom.foundryCelebrationLayer = document.getElementById("foundryCelebrationLayer");
    dom.foundryCelebrationBurstHost = document.getElementById("foundryCelebrationBurstHost");
    dom.globalConfettiHost = document.getElementById("globalConfettiHost");
    dom.foundryConfettiEmitterHandles = Array.from(document.querySelectorAll(".confetti-emitter-handle"));
    dom.foundryFactoryRoundabout = document.getElementById("foundryFactoryRoundabout");
    dom.foundryFactoryStartNode = document.getElementById("foundryFactoryStartNode");
    dom.foundryPickupNodes = Array.from(document.querySelectorAll(".map-pickup-node"));
    dom.foundryFleetYard = document.getElementById("foundryFleetYard");
    dom.foundryDeleteCan = document.getElementById("foundryDeleteCan");
    dom.foundryEditSandlot = document.getElementById("foundryEditSandlot");
    dom.foundryMapNodes = Array.from(document.querySelectorAll(".map-stage--foundry .map-node"));
    dom.foundryCampusAssets = Array.from(document.querySelectorAll(".lmsa-campus-asset"));
    dom.undoRouteButton = document.getElementById("undoRouteButton");
    dom.clearBusScheduleButton = document.getElementById("clearBusScheduleButton");
    dom.restartLevelButton = document.getElementById("restartLevelButton");
    dom.submitSolutionButton = document.getElementById("submitSolutionButton");
    dom.toastRegion = document.getElementById("toastRegion");
    dom.gameVictoryOverlay = document.getElementById("gameVictoryOverlay");
    dom.gameVictoryScoreline = dom.gameVictoryOverlay?.querySelector(".game-victory__scoreline") || null;
    dom.gameVictoryTitle = document.getElementById("gameVictoryTitle");
    dom.gameVictoryStars = document.getElementById("gameVictoryStars");
    dom.gameVictoryScaleFill = document.getElementById("gameVictoryScaleFill");
    dom.gameVictoryMarkerOptimal = document.getElementById("gameVictoryMarkerOptimal");
    dom.gameVictoryMarkerBudget = document.getElementById("gameVictoryMarkerBudget");
    dom.gameVictoryMarkerThreshold = document.getElementById("gameVictoryMarkerThreshold");
    dom.gameVictoryPlayerMarker = document.getElementById("gameVictoryPlayerMarker");
    dom.gameVictoryPlayerCost = document.getElementById("gameVictoryPlayerCost");
    dom.gameVictoryBudget = document.getElementById("gameVictoryBudget");
    dom.gameVictoryOptimal = document.getElementById("gameVictoryOptimal");
    dom.gameVictoryThreshold = document.getElementById("gameVictoryThreshold");
    dom.gameVictoryBusRentalCost = document.getElementById("gameVictoryBusRentalCost");
    dom.gameVictoryOpportunityCost = document.getElementById("gameVictoryOpportunityCost");
    dom.gameVictoryTotalCost = document.getElementById("gameVictoryTotalCost");
    dom.gameVictoryMetricYourCost = document.getElementById("gameVictoryMetricYourCost");
    dom.gameVictoryMetricOptimalCost = document.getElementById("gameVictoryMetricOptimalCost");
    dom.gameVictoryGap = document.getElementById("gameVictoryGap");
    dom.gameVictoryGapMetric = document.getElementById("gameVictoryGapMetric");
    dom.gameVictoryViewSolutionReplayButton = document.getElementById("gameVictoryViewSolutionReplayButton");
    dom.gameVictoryViewOptimalReplayButton = document.getElementById("gameVictoryViewOptimalReplayButton");
    dom.gameVictoryNextLevelButton = document.getElementById("gameVictoryNextLevelButton");
    dom.gameVictoryReplayButton = document.getElementById("gameVictoryReplayButton");
    dom.gameVictoryLevelSelectButton = document.getElementById("gameVictoryLevelSelectButton");
  }

  function getLevelData(levelName) {
    return LEVELS.find((level) => level.name === levelName) || LEVELS[0];
  }

  function isLevelComingSoon(levelOrName) {
    const level = typeof levelOrName === "string" ? getLevelData(levelOrName) : levelOrName;
    return Boolean(level?.isComingSoon);
  }

  function getLevelLockedToastMessage(levelOrName) {
    const level = typeof levelOrName === "string" ? getLevelData(levelOrName) : levelOrName;
    return level?.lockedToastMessage || "This level is still warming up at LMSA. It will be ready soon enough.";
  }

  function escapeHtml(value) {
    return String(value)
      .replaceAll("&", "&amp;")
      .replaceAll("<", "&lt;")
      .replaceAll(">", "&gt;")
      .replaceAll('"', "&quot;")
      .replaceAll("'", "&#39;");
  }

  function renderBulletList(listElement, items) {
    if (!listElement) {
      return;
    }

    listElement.innerHTML = items.map((item) => `<li>${escapeHtml(item)}</li>`).join("");
  }

  function renderFeaturedLevel() {
    const level = getLevelData(DEFAULT_LEVEL);

    if (dom.featuredLevelTitle) {
      dom.featuredLevelTitle.textContent = level.name;
    }

    if (dom.featuredLevelDescription) {
      dom.featuredLevelDescription.textContent = level.description;
    }

    if (dom.featuredBudget) {
      dom.featuredBudget.textContent = level.benchmark;
    }

    if (dom.featuredDifficulty) {
      dom.featuredDifficulty.textContent = level.difficulty;
    }

    renderBulletList(dom.featuredRuleList, level.featuredRules);
  }

  function getGuideReplayRouteRecord() {
    return FOUNDRY_OPTIMAL_SCHEDULE_REPLAY_SNAPSHOTS[GUIDE_REPLAY_LEVEL_NAME]?.buses?.[0]?.completedRouteHistory?.[0] || null;
  }

  function isGuideReplayPickupLabel(label) {
    return String(label || "").startsWith("pickup-");
  }

  function getGuideReplayNodeLayout(label) {
    const safeLabel = String(label || "");

    if (!safeLabel) {
      return null;
    }

    if (safeLabel === FOUNDRY_FACTORY_START_LABEL) {
      const factoryPlacement = FOUNDRY_FALLBACK_CAMPUS_ASSETS.factory;
      return factoryPlacement
        ? { label: safeLabel, type: "factory", left: factoryPlacement.left, top: factoryPlacement.top }
        : null;
    }

    const pickupConfig = FOUNDRY_LEVEL_PICKUP_NODE_CONFIGS[GUIDE_REPLAY_LEVEL_NAME]?.[safeLabel];

    if (pickupConfig) {
      return {
        label: safeLabel,
        type: "pickup",
        left: Number(pickupConfig.anchorLeft) || 0,
        top: Number(pickupConfig.anchorTop) || 0,
        demand: Math.max(Number(FOUNDRY_LEVEL_STOP_BASE_DEMANDS[GUIDE_REPLAY_LEVEL_NAME]?.[safeLabel]) || 0, 0),
      };
    }

    const nodePlacement = FOUNDRY_FALLBACK_NODE_POSITIONS[safeLabel];

    if (!nodePlacement) {
      return null;
    }

    return {
      label: safeLabel,
      type: "intersection",
      left: Number(nodePlacement.left) || 0,
      top: Number(nodePlacement.top) || 0,
    };
  }

  function appendGuideReplayRoadSegment(roadLayer, fromPoint, toPoint) {
    if (!roadLayer || !fromPoint || !toPoint) {
      return;
    }

    const line = document.createElementNS("http://www.w3.org/2000/svg", "line");
    line.setAttribute("x1", String(fromPoint.left));
    line.setAttribute("y1", String(fromPoint.top));
    line.setAttribute("x2", String(toPoint.left));
    line.setAttribute("y2", String(toPoint.top));
    line.setAttribute("class", "guide-replay-demo__road");
    roadLayer.append(line);
  }

  function renderGuideReplayStage() {
    const stage = dom.guideReplayStage;
    const roadLayer = dom.guideReplayRoadLayer;
    const nodeLayer = dom.guideReplayNodeLayer;
    const routeRecord = getGuideReplayRouteRecord();
    const routeNodeLabels = Array.isArray(routeRecord?.routeNodeLabels) ? routeRecord.routeNodeLabels : [];

    if (!stage || !roadLayer || !nodeLayer || routeNodeLabels.length < 2) {
      return;
    }

    roadLayer.replaceChildren();
    nodeLayer.replaceChildren();

    for (let index = 1; index < routeNodeLabels.length; index += 1) {
      appendGuideReplayRoadSegment(
        roadLayer,
        getGuideReplayNodeLayout(routeNodeLabels[index - 1]),
        getGuideReplayNodeLayout(routeNodeLabels[index]),
      );
    }

    Array.from(new Set(routeNodeLabels)).forEach((label) => {
      const nodeLayout = getGuideReplayNodeLayout(label);

      if (!nodeLayout) {
        return;
      }

      if (nodeLayout.type === "factory") {
        const plant = document.createElement("div");
        plant.className = "guide-replay-demo__plant";
        plant.style.left = `${nodeLayout.left}%`;
        plant.style.top = `${nodeLayout.top}%`;
        plant.innerHTML = `
          <img
            class="guide-replay-demo__plant-image"
            src="LMSA%20Factory.png"
            alt=""
            draggable="false"
          >
        `;
        nodeLayer.append(plant);
        return;
      }

      const nodeElement = document.createElement("span");
      nodeElement.className = `guide-replay-demo__node guide-replay-demo__node--${nodeLayout.type === "pickup" ? "bus-stop" : "intersection"}`;
      nodeElement.style.left = `${nodeLayout.left}%`;
      nodeElement.style.top = `${nodeLayout.top}%`;

      if (nodeLayout.type === "pickup") {
        nodeElement.dataset.guideReplayStop = nodeLayout.label;
        nodeElement.innerHTML = `
          <span class="guide-replay-demo__stop-demand" aria-hidden="true">
            <span class="guide-replay-demo__stop-icon">
              <img
                class="guide-replay-demo__stop-icon-image"
                src="LMSA%20Worker%20Head.png"
                alt=""
                draggable="false"
              >
            </span>
            <span class="guide-replay-demo__stop-count">x${escapeHtml(nodeLayout.demand || 1)}</span>
          </span>
        `;
      }

      nodeLayer.append(nodeElement);
    });
  }

  function getGuideReplayTravelDurationMs(label) {
    return Math.max(
      (isGuideReplayPickupLabel(label) ? FOUNDRY_PICKUP_NODE_MINUTES : FOUNDRY_ROUTE_MINUTES_PER_EDGE) * GUIDE_REPLAY_MS_PER_MINUTE,
      1,
    );
  }

  function buildGuideReplayTimeline(routeRecord = getGuideReplayRouteRecord()) {
    const routeNodeLabels = Array.isArray(routeRecord?.routeNodeLabels) ? routeRecord.routeNodeLabels : [];
    const phases = [];
    let totalDurationMs = 0;

    for (let index = 1; index < routeNodeLabels.length; index += 1) {
      const previousLabel = routeNodeLabels[index - 1];
      const currentLabel = routeNodeLabels[index];
      const fromPoint = getGuideReplayNodeLayout(previousLabel);
      const toPoint = getGuideReplayNodeLayout(currentLabel);

      if (!fromPoint || !toPoint) {
        continue;
      }

      const travelDurationMs = getGuideReplayTravelDurationMs(currentLabel);
      phases.push({
        type: "travel",
        fromPoint,
        toPoint,
        durationMs: travelDurationMs,
      });
      totalDurationMs += travelDurationMs;

      let pauseDurationMs = 0;
      let servedStopLabel = "";

      if (isGuideReplayPickupLabel(currentLabel)) {
        pauseDurationMs = GUIDE_REPLAY_PICKUP_PAUSE_MS;
        servedStopLabel = String(currentLabel);
      } else if (currentLabel === FOUNDRY_FACTORY_START_LABEL) {
        pauseDurationMs = GUIDE_REPLAY_DEPOT_PAUSE_MS;
      }

      if (pauseDurationMs > 0) {
        phases.push({
          type: "pause",
          fromPoint,
          toPoint,
          position: toPoint,
          durationMs: pauseDurationMs,
          servedStopLabel,
        });
        totalDurationMs += pauseDurationMs;
      }
    }

    return {
      phases,
      totalDurationMs,
      startPoint: getGuideReplayNodeLayout(routeNodeLabels[0]),
    };
  }

  function setGuideReplayServedStops(servedStops = new Set()) {
    dom.guideReplayNodeLayer?.querySelectorAll("[data-guide-replay-stop]").forEach((stopNode) => {
      stopNode.classList.toggle("is-served", servedStops.has(stopNode.dataset.guideReplayStop || ""));
    });
  }

  function getGuideReplayAngleDegrees(fromPoint, toPoint) {
    const stageRect = dom.guideReplayStage?.getBoundingClientRect();

    if (!stageRect || !stageRect.width || !stageRect.height || !fromPoint || !toPoint) {
      return 0;
    }

    const deltaX = ((Number(toPoint.left) || 0) - (Number(fromPoint.left) || 0)) * stageRect.width;
    const deltaY = ((Number(toPoint.top) || 0) - (Number(fromPoint.top) || 0)) * stageRect.height;
    return Math.atan2(deltaY, deltaX) * (180 / Math.PI);
  }

  function renderGuideReplayRunner(position, angleDegrees = 0) {
    const runner = dom.guideReplayRunner;
    const runnerBody = dom.guideReplayRunnerBody;

    if (!runner || !runnerBody || !position) {
      return;
    }

    const shouldFlipLeft = angleDegrees > 90 || angleDegrees < -90;
    const displayAngleDegrees = shouldFlipLeft
      ? angleDegrees + (angleDegrees > 90 ? -180 : 180)
      : angleDegrees;

    runner.style.left = `${position.left}%`;
    runner.style.top = `${position.top}%`;
    runner.style.transform = `translate(-50%, -50%) rotate(${displayAngleDegrees}deg)`;
    runnerBody.style.transform = shouldFlipLeft ? "scale(-0.7, 0.7)" : "scale(0.7)";
    runner.classList.remove("is-hidden");
  }

  function getGuideReplayFrameState(timeline, elapsedMs) {
    const servedStops = new Set();
    let traversedMs = 0;

    for (let index = 0; index < timeline.phases.length; index += 1) {
      const phase = timeline.phases[index];
      const phaseEndMs = traversedMs + phase.durationMs;

      if (elapsedMs <= phaseEndMs) {
        if (phase.type === "travel") {
          const progress = phase.durationMs ? clamp((elapsedMs - traversedMs) / phase.durationMs, 0, 1) : 1;
          return {
            position: {
              left: phase.fromPoint.left + (phase.toPoint.left - phase.fromPoint.left) * progress,
              top: phase.fromPoint.top + (phase.toPoint.top - phase.fromPoint.top) * progress,
            },
            angleDegrees: getGuideReplayAngleDegrees(phase.fromPoint, phase.toPoint),
            servedStops,
          };
        }

        if (phase.servedStopLabel) {
          servedStops.add(phase.servedStopLabel);
        }

        return {
          position: phase.position,
          angleDegrees: getGuideReplayAngleDegrees(phase.fromPoint, phase.toPoint),
          servedStops,
        };
      }

      if (phase.servedStopLabel) {
        servedStops.add(phase.servedStopLabel);
      }

      traversedMs = phaseEndMs;
    }

    return {
      position: timeline.startPoint,
      angleDegrees: 0,
      servedStops,
    };
  }

  function startGuideReplayDemo() {
    if (state.guideReplayFrame) {
      window.cancelAnimationFrame(state.guideReplayFrame);
      state.guideReplayFrame = 0;
    }

    state.guideReplayTimeline = buildGuideReplayTimeline();

    if (!dom.guideReplayStage || !dom.guideReplayRunner || !state.guideReplayTimeline?.phases?.length) {
      return;
    }

    state.guideReplayStartTime = 0;
    state.guideReplayCycleIndex = -1;
    setGuideReplayServedStops(new Set());
    dom.guideReplayRunner.classList.add("is-hidden");

    const step = (timestamp) => {
      if (!state.guideReplayStartTime) {
        state.guideReplayStartTime = timestamp;
      }

      const timeline = state.guideReplayTimeline;
      const cycleDurationMs = timeline.totalDurationMs + GUIDE_REPLAY_LOOP_GAP_MS;
      const elapsedMs = timestamp - state.guideReplayStartTime;
      const cycleIndex = Math.floor(elapsedMs / cycleDurationMs);
      const cycleElapsedMs = elapsedMs % cycleDurationMs;

      if (cycleIndex !== state.guideReplayCycleIndex) {
        state.guideReplayCycleIndex = cycleIndex;
        setGuideReplayServedStops(new Set());
        dom.guideReplayRunner.classList.add("is-hidden");
      }

      if (cycleElapsedMs <= timeline.totalDurationMs) {
        const frameState = getGuideReplayFrameState(timeline, cycleElapsedMs);
        setGuideReplayServedStops(frameState.servedStops);
        renderGuideReplayRunner(frameState.position, frameState.angleDegrees);
      } else {
        dom.guideReplayRunner.classList.add("is-hidden");
      }

      state.guideReplayFrame = window.requestAnimationFrame(step);
    };

    state.guideReplayFrame = window.requestAnimationFrame(step);
  }

  function stopGuideReplayDemo(options = {}) {
    const { reset = false } = options;

    if (state.guideReplayFrame) {
      window.cancelAnimationFrame(state.guideReplayFrame);
      state.guideReplayFrame = 0;
    }

    state.guideReplayStartTime = 0;
    state.guideReplayCycleIndex = -1;

    if (reset) {
      setGuideReplayServedStops(new Set());
      dom.guideReplayRunner?.classList.add("is-hidden");
    }
  }

  function initializeGuideReplayDemo(options = {}) {
    const { autoplay = false } = options;

    renderGuideReplayStage();

    if (autoplay) {
      startGuideReplayDemo();
      return;
    }

    stopGuideReplayDemo({ reset: true });
  }

  function renderLevelGrid() {
    if (!dom.levelGrid) {
      return;
    }

    dom.levelGrid.innerHTML = LEVELS.map(
      (level) => `
        <article class="level-card panel${level.isComingSoon ? " is-coming-soon" : ""}">
          <h3>${escapeHtml(level.levelSelectTitle || level.name)}</h3>
          <p>${escapeHtml(level.levelSelectDescription || level.description)}</p>
          <div class="level-card__meta">
            <div class="meta-box">
              <span>Budget</span>
              <strong>${escapeHtml(level.benchmark)}</strong>
            </div>
            <div class="meta-box">
              <span>Difficulty</span>
              <strong>${escapeHtml(level.difficulty)}</strong>
            </div>
          </div>
          <button class="button ${level.isComingSoon ? "button--ghost" : "button--primary"} level-open" type="button" data-level="${escapeHtml(level.name)}">${level.isComingSoon ? "Coming Soon" : "Open"}</button>
        </article>
      `,
    ).join("");
  }

  function formatLevelHeader(levelName) {
    const [levelLabel, ...headerRemainder] = String(levelName).split(" - ");
    const trimmedLevelLabel = levelLabel.trim();
    const trimmedLevelTitle = headerRemainder.join(" - ").trim();

    return trimmedLevelTitle ? `${trimmedLevelLabel}: ${trimmedLevelTitle}` : trimmedLevelLabel;
  }

  function formatFleetSequence(sequence) {
    return String(sequence).padStart(2, "0");
  }

  function getBusShopConfig(type) {
    return BUS_SHOP_CONFIG[type] || BUS_SHOP_CONFIG.lift;
  }

  function formatSelectedBusCode(bus) {
    const config = getBusShopConfig(bus?.type);
    const sequence = Math.max(Math.round(Number(bus?.sequence) || 0), 0);
    return `${config.shortCode || config.badge || "B"}${sequence}`;
  }

  function getSelectedFleetBus() {
    return state.purchasedFleet.find((bus) => bus.id === state.selectedFleetBusId) || null;
  }

  function isFoundryRouteAnimationActive() {
    return Boolean(state.foundryAnimatingBusId);
  }

  function getFoundryLevelStopBaseDemands(levelName = state.currentLevel) {
    const targetLevelName = String(levelName || DEFAULT_LEVEL);
    return FOUNDRY_LEVEL_STOP_BASE_DEMANDS[targetLevelName] || {};
  }

  function getFoundryLevelPickupConfigs(levelName = state.currentLevel) {
    const targetLevelName = String(levelName || DEFAULT_LEVEL);
    return FOUNDRY_LEVEL_PICKUP_NODE_CONFIGS[targetLevelName] || {};
  }

  function getFoundryLegacyPickupConfig(label) {
    return FOUNDRY_LEVEL_PICKUP_NODE_CONFIGS[DEFAULT_LEVEL]?.[String(label || "")] || null;
  }

  function getFoundryLegacyPickupDemand(label) {
    return Math.max(Number(FOUNDRY_LEVEL_STOP_BASE_DEMANDS[DEFAULT_LEVEL]?.[String(label || "")]) || 0, 0);
  }

  function isFoundryPickupRouteLabel(label) {
    return Object.prototype.hasOwnProperty.call(getFoundryLevelPickupConfigs(), String(label || ""));
  }

  function getFoundryPickupConfig(label) {
    return getFoundryLevelPickupConfigs()[String(label || "")] || null;
  }

  function getFoundryDemandNodeRecord(label) {
    const routeLabel = String(label || "");

    if (isFoundryPickupRouteLabel(routeLabel)) {
      return {
        label: routeLabel,
        variant: "blue",
        demand: Math.max(Number(getFoundryLevelStopBaseDemands()[routeLabel]) || 0, 0),
        isStaticPickup: true,
      };
    }

    const customNodeRecord = state.foundryCustomNodes.find(
      (nodeRecord) => nodeRecord.variant === "blue" && nodeRecord.label === routeLabel,
    );

    if (!customNodeRecord) {
      return null;
    }

    return customNodeRecord;
  }

  function isFoundryDemandRouteLabel(label) {
    return Boolean(getFoundryDemandNodeRecord(label));
  }

  function getFoundryRouteSelectionCost(label) {
    return isFoundryDemandRouteLabel(label) ? FOUNDRY_PICKUP_NODE_MINUTES : FOUNDRY_ROUTE_MINUTES_PER_EDGE;
  }

  function calculateFoundryRouteMinutes(routeNodeLabels) {
    return (Array.isArray(routeNodeLabels) ? routeNodeLabels : []).reduce((totalMinutes, label, index) => {
      if (index === 0) {
        return totalMinutes;
      }

      return totalMinutes + getFoundryRouteSelectionCost(label);
    }, 0);
  }

  function getFleetBusRouteNodeLabels(bus) {
    return Array.isArray(bus?.routeNodeLabels) ? bus.routeNodeLabels : [];
  }

  function getFleetBusDraftRouteMinutes(bus) {
    return calculateFoundryRouteMinutes(getFleetBusRouteNodeLabels(bus));
  }

  function getFleetBusProjectedRouteMinutes(bus, nextLabel = "") {
    const routeNodeLabels = getFleetBusRouteNodeLabels(bus);

    if (!nextLabel) {
      return calculateFoundryRouteMinutes(routeNodeLabels);
    }

    return calculateFoundryRouteMinutes([...routeNodeLabels, String(nextLabel)]);
  }

  function roundCurrencyAmount(amount) {
    return Math.round((Number(amount) + Number.EPSILON) * 100) / 100;
  }

  function parseDollarAmount(value) {
    const numericValue = Number.parseFloat(String(value ?? "").replace(/[^0-9.-]/g, ""));
    return Number.isFinite(numericValue) ? numericValue : 0;
  }

  function formatPercentAmount(value) {
    return `${Math.max(Number(value) || 0, 0).toFixed(1)}%`;
  }

  function waitForMs(durationMs) {
    return new Promise((resolve) => {
      window.setTimeout(resolve, Math.max(Number(durationMs) || 0, 0));
    });
  }

  function getFleetTotalCost() {
    return roundCurrencyAmount(
      state.purchasedFleet.reduce((totalCost, bus) => {
        return totalCost + (Number(bus?.totalCost) || Number(bus?.purchaseCost) || 0);
      }, 0),
    );
  }

  function getFleetRentalCost() {
    return roundCurrencyAmount(
      state.purchasedFleet.reduce((totalCost, bus) => {
        return totalCost + (Number(bus?.purchaseCost) || 0);
      }, 0),
    );
  }

  function getFleetOpportunityCost() {
    return roundCurrencyAmount(
      state.purchasedFleet.reduce((totalCost, bus) => {
        const purchaseCost = Number(bus?.purchaseCost) || 0;
        const accumulatedCost = Number(bus?.totalCost) || purchaseCost;
        return totalCost + Math.max(accumulatedCost - purchaseCost, 0);
      }, 0),
    );
  }

  function getFleetBusCompletedRouteHistory(bus) {
    return Array.isArray(bus?.completedRouteHistory) ? bus.completedRouteHistory : [];
  }

  function cloneFoundryReplayRouteRecord(routeRecord) {
    const pickupPlan = Object.entries(routeRecord?.pickupPlan || {}).reduce((plan, [routeIndex, boardedWorkers]) => {
      const safeBoardedWorkers = Math.max(Number(boardedWorkers) || 0, 0);

      if (safeBoardedWorkers > 0) {
        plan[String(routeIndex)] = safeBoardedWorkers;
      }

      return plan;
    }, {});

    const stopCounts = Object.entries(routeRecord?.stopCounts || {}).reduce((counts, [stopAsset, count]) => {
      const safeCount = Math.max(Number(count) || 0, 0);

      if (safeCount > 0) {
        counts[String(stopAsset)] = safeCount;
      }

      return counts;
    }, {});

    return {
      routeNodeLabels: Array.isArray(routeRecord?.routeNodeLabels)
        ? routeRecord.routeNodeLabels.map((label) => String(label))
        : [],
      startMinute: Math.max(Number(routeRecord?.startMinute) || 0, 0),
      durationMinutes: Math.max(Number(routeRecord?.durationMinutes) || 0, 0),
      pickupPlan,
      stopCounts,
    };
  }

  function buildFoundryScheduleReplaySnapshot() {
    return {
      version: 1,
      levelName: state.currentLevel,
      capturedAt: Date.now(),
      buses: state.purchasedFleet.map((bus) => ({
        id: String(bus?.id || ""),
        sequence: getFleetBusSequence(bus),
        type: String(bus?.type || ""),
        purchaseCost: roundCurrencyAmount(Number(bus?.purchaseCost) || 0),
        capacity: Math.max(Number(bus?.capacity) || 0, 0),
        totalCost: roundCurrencyAmount(Number(bus?.totalCost) || 0),
        completedRouteHistory: getFleetBusCompletedRouteHistory(bus).map(cloneFoundryReplayRouteRecord),
      })),
    };
  }

  function saveFoundryScheduleReplaySnapshot(storageKey = FOUNDRY_SCHEDULE_REPLAY_STORAGE_KEY) {
    try {
      localStorage.setItem(storageKey, JSON.stringify(buildFoundryScheduleReplaySnapshot()));
    } catch (_) { /* ignore */ }
  }

  function calculateCompletedRouteOpportunityCost(bus) {
    const currentCommittedDutyTimeMinutes = Number.isFinite(bus?.committedDutyTimeMinutes)
      ? bus.committedDutyTimeMinutes
      : 0;
    const completedCommittedDutyTimeMinutes = currentCommittedDutyTimeMinutes + getFleetBusDraftRouteMinutes(bus);
    const routeLoad = Math.max(Number(bus?.load) || 0, 0);
    const minuteRate = FOUNDRY_OPPORTUNITY_COST_PER_HOUR / 60;

    return roundCurrencyAmount(completedCommittedDutyTimeMinutes * routeLoad * minuteRate);
  }

  function getLevelBudgetAmount(level = getLevelData(state.currentLevel)) {
    return parseDollarAmount(level?.benchmark);
  }

  function getLevelOptimalAmount(level = getLevelData(state.currentLevel)) {
    return parseDollarAmount(level?.results?.optimalCost) || getLevelBudgetAmount(level);
  }

  function calculateVictoryStarCount(totalCost, budgetAmount, optimalAmount) {
    const roundedTotalCost = roundCurrencyAmount(totalCost);
    const roundedBudgetAmount = roundCurrencyAmount(budgetAmount);
    const roundedOptimalAmount = roundCurrencyAmount(optimalAmount);

    if (roundedTotalCost <= roundedOptimalAmount + 0.005) {
      return 3;
    }

    if (roundedTotalCost <= roundedBudgetAmount + 0.005) {
      return 2;
    }

    if (roundedTotalCost <= roundCurrencyAmount(roundedBudgetAmount * ONE_STAR_THRESHOLD_MULTIPLIER) + 0.005) {
      return 1;
    }

    return 0;
  }

  function getNextLevelData(levelName = state.currentLevel) {
    const currentLevelIndex = LEVELS.findIndex((level) => level.name === levelName);

    if (currentLevelIndex < 0) {
      return null;
    }

    for (let index = currentLevelIndex + 1; index < LEVELS.length; index += 1) {
      if (!isLevelComingSoon(LEVELS[index])) {
        return LEVELS[index];
      }
    }

    return null;
  }

  function buildVictoryScale(budgetAmount, optimalAmount) {
    const safeBudget = Number(budgetAmount) || 0;
    const safeOptimal = Number.isFinite(Number(optimalAmount)) ? Number(optimalAmount) : safeBudget;

    return {
      leftAmount: roundCurrencyAmount(Math.max(safeBudget * VICTORY_SCALE_LEFT_BUDGET_MULTIPLIER, safeBudget)),
      rightAmount: roundCurrencyAmount(safeOptimal),
      budgetAmount: safeBudget,
      budgetPercent: VICTORY_SCALE_BUDGET_PERCENT,
    };
  }

  function calculateVictoryScalePercent(amount, scale) {
    if (!scale) {
      return 0;
    }

    const leftAmount = Number(scale.leftAmount);
    const rightAmount = Number(scale.rightAmount);
    const budgetAmount = Number(scale.budgetAmount);
    const budgetPercent = clamp(Number(scale.budgetPercent) || 0, 0, 100);
    const amountValue = Number(amount) || 0;

    if (!Number.isFinite(leftAmount) || !Number.isFinite(rightAmount) || !Number.isFinite(budgetAmount)) {
      return 0;
    }

    const isBetween = (value, start, end) => (value - start) * (value - end) <= 0;

    let percent = budgetPercent;

    if (leftAmount !== budgetAmount && isBetween(amountValue, leftAmount, budgetAmount)) {
      percent = ((amountValue - leftAmount) / (budgetAmount - leftAmount)) * budgetPercent;
    } else if (budgetAmount !== rightAmount) {
      percent =
        budgetPercent +
        ((amountValue - budgetAmount) / (rightAmount - budgetAmount)) * (100 - budgetPercent);
    } else if (leftAmount !== budgetAmount) {
      percent = ((amountValue - leftAmount) / (budgetAmount - leftAmount)) * budgetPercent;
    }

    return clamp(percent, 0, 100);
  }

  function buildCurrentLevelOutcome() {
    const level = getLevelData(state.currentLevel);
    const busRentalCost = getFleetRentalCost();
    const opportunityCost = getFleetOpportunityCost();
    const totalCost = roundCurrencyAmount(busRentalCost + opportunityCost);
    const budgetAmount = getLevelBudgetAmount(level);
    const optimalAmount = getLevelOptimalAmount(level);
    const oneStarThresholdAmount = roundCurrencyAmount(budgetAmount * ONE_STAR_THRESHOLD_MULTIPLIER);
    const starsEarned = calculateVictoryStarCount(totalCost, budgetAmount, optimalAmount);
    const fleetUsedCount = state.purchasedFleet.length;
    const gapPercent = optimalAmount > 0
      ? formatPercentAmount(((totalCost - optimalAmount) / optimalAmount) * 100)
      : "0.0%";
    const nextLevel = getNextLevelData(level.name);
    const victoryScale = buildVictoryScale(budgetAmount, optimalAmount);

    let title = "You're Fired :)";
    let summary = `Everyone made it back to LMSA, but this plan finished above the ${formatDollarAmount(oneStarThresholdAmount)} one-star line.`;
    let highlights = [
      "All riders returned to LMSA.",
      `${fleetUsedCount} ${fleetUsedCount === 1 ? "bus" : "buses"} activated.`,
      `One-star cutoff sits at ${formatDollarAmount(oneStarThresholdAmount)} for this level.`,
    ];

    if (starsEarned === 3) {
      title = "Optimal Match";
      summary = "You matched the optimal known solution for this level and cleared the entire morning plan at peak efficiency.";
      highlights = [
        "All riders returned to LMSA.",
        `${fleetUsedCount} ${fleetUsedCount === 1 ? "bus" : "buses"} activated.`,
        `You matched the optimal target of ${formatDollarAmount(optimalAmount)}.`,
      ];
    } else if (starsEarned === 2) {
      title = "Under Budget";
      summary = "You cleared the level and stayed under budget. There is still a little room to tighten the schedule and chase the optimal plan.";
      highlights = [
        "All riders returned to LMSA.",
        `${fleetUsedCount} ${fleetUsedCount === 1 ? "bus" : "buses"} activated.`,
        `You stayed inside the ${formatDollarAmount(budgetAmount)} budget target.`,
      ];
    } else if (starsEarned === 1) {
      title = "Level Cleared";
      summary = "You finished the level successfully, but the plan ran above budget and landed in the one-star band.";
      highlights = [
        "All riders returned to LMSA.",
        `${fleetUsedCount} ${fleetUsedCount === 1 ? "bus" : "buses"} activated.`,
        `You still cleared the ${formatDollarAmount(oneStarThresholdAmount)} one-star cutoff.`,
      ];
    }

    return {
      level,
      totalCost,
      busRentalCost,
      opportunityCost,
      budgetAmount,
      optimalAmount,
      oneStarThresholdAmount,
      victoryScale,
      starsEarned,
      fleetUsedCount,
      gapPercent,
      nextLevel,
      title,
      summary,
      highlights,
    };
  }

  function renderVictoryStars(starsEarned) {
    if (!dom.gameVictoryStars) {
      return;
    }

    if (dom.gameVictoryStars.children.length !== 3) {
      dom.gameVictoryStars.innerHTML = "";

      for (let index = 0; index < 3; index += 1) {
        const star = document.createElement("span");
        star.className = "game-victory__star";
        star.textContent = STAR_FULL;
        dom.gameVictoryStars.append(star);
      }
    }

    const safeStarsEarned = clamp(Math.round(Number(starsEarned) || 0), 0, 3);
    const starNodes = dom.gameVictoryStars.querySelectorAll(".game-victory__star");
    starNodes.forEach((star, index) => {
      star.classList.toggle("is-earned", index < safeStarsEarned);
    });
    dom.gameVictoryStars.setAttribute("aria-label", `${safeStarsEarned} out of 3 stars earned`);
  }

  function positionVictoryScaleElement(element, amount, scale) {
    if (!element) {
      return 0;
    }

    const percent = calculateVictoryScalePercent(amount, scale);
    element.style.left = `${percent}%`;
    return percent;
  }

  function cancelVictoryScoreAnimation() {
    if (state.victoryScoreAnimationFrame) {
      window.cancelAnimationFrame(state.victoryScoreAnimationFrame);
      state.victoryScoreAnimationFrame = 0;
    }
  }

  function setVictoryScoreProgress(percent) {
    const safePercent = clamp(Number(percent) || 0, 0, 100);

    if (dom.gameVictoryPlayerMarker) {
      dom.gameVictoryPlayerMarker.style.left = `${safePercent}%`;
    }

    if (dom.gameVictoryScaleFill) {
      dom.gameVictoryScaleFill.style.width = `${safePercent}%`;
    }
  }

  function setVictoryDelayedRevealHidden(isHidden) {
    const shouldHide = Boolean(isHidden);
    const delayedRevealNodes = [
      dom.gameVictoryTitle,
      dom.gameVictoryMetricYourCost,
      dom.gameVictoryMetricOptimalCost,
      dom.gameVictoryGap,
      dom.gameVictoryBusRentalCost,
      dom.gameVictoryOpportunityCost,
      dom.gameVictoryTotalCost,
    ].filter(Boolean);

    delayedRevealNodes.forEach((node) => {
      node.classList.toggle("is-awaiting-reveal", shouldHide);
    });
  }

  function prepareVictoryScoreAnimation() {
    cancelVictoryScoreAnimation();
    setVictoryDelayedRevealHidden(true);
    dom.gameVictoryScoreline?.classList.add("is-score-reset");
    setVictoryScoreProgress(0);
    renderVictoryStars(0);
    void dom.gameVictoryScoreline?.offsetHeight;
  }

  function getVictoryStarsAtProgress(progressPercent, benchmarkPercents, finalStarsEarned) {
    const currentProgress = clamp(Number(progressPercent) || 0, 0, 100);
    const oneStarPercent = Number(benchmarkPercents?.oneStarPercent) || 0;
    const budgetPercent = Number(benchmarkPercents?.budgetPercent) || 0;
    const optimalPercent = Number(benchmarkPercents?.optimalPercent) || 0;
    let starsEarned = 0;

    if (currentProgress >= oneStarPercent - 0.001) {
      starsEarned += 1;
    }

    if (currentProgress >= budgetPercent - 0.001) {
      starsEarned += 1;
    }

    if (currentProgress >= optimalPercent - 0.001) {
      starsEarned += 1;
    }

    return Math.min(starsEarned, clamp(Math.round(Number(finalStarsEarned) || 0), 0, 3));
  }

  function runVictoryScoreAnimation(animation) {
    cancelVictoryScoreAnimation();
    const targetPercent = clamp(Number(animation?.playerPercent) || 0, 0, 100);
    const benchmarkPercents = {
      oneStarPercent: Number(animation?.oneStarPercent) || 0,
      budgetPercent: Number(animation?.budgetPercent) || 0,
      optimalPercent: Number(animation?.optimalPercent) || 0,
    };
    const finalStarsEarned = clamp(Math.round(Number(animation?.finalStarsEarned) || 0), 0, 3);

    if (window.matchMedia?.("(prefers-reduced-motion: reduce)")?.matches) {
      setVictoryScoreProgress(targetPercent);
      renderVictoryStars(finalStarsEarned);
      dom.gameVictoryScoreline?.classList.remove("is-score-reset");
      setVictoryDelayedRevealHidden(false);
      return;
    }

    let startTime = 0;
    let previousStars = -1;

    const animate = (timestamp) => {
      if (!startTime) {
        startTime = timestamp;
      }

      const elapsed = timestamp - startTime;
      const progress = clamp(elapsed / VICTORY_SCORE_ANIMATION_DURATION_MS, 0, 1);
      const easedProgress = 1 - ((1 - progress) ** 2);
      const currentPercent = targetPercent * easedProgress;
      const starsAtProgress = getVictoryStarsAtProgress(currentPercent, benchmarkPercents, finalStarsEarned);

      setVictoryScoreProgress(currentPercent);

      if (starsAtProgress !== previousStars) {
        renderVictoryStars(starsAtProgress);
        previousStars = starsAtProgress;
      }

      if (progress < 1) {
        state.victoryScoreAnimationFrame = window.requestAnimationFrame(animate);
        return;
      }

      setVictoryScoreProgress(targetPercent);
      renderVictoryStars(finalStarsEarned);
      dom.gameVictoryScoreline?.classList.remove("is-score-reset");
      setVictoryDelayedRevealHidden(false);
      state.victoryScoreAnimationFrame = 0;
    };

    state.victoryScoreAnimationFrame = window.requestAnimationFrame(animate);
  }

  function applyOutcomeToResultsPanels(outcome) {
    if (!outcome) {
      return;
    }

    if (dom.resultPlayerCost) {
      dom.resultPlayerCost.textContent = formatDollarAmount(outcome.totalCost);
    }

    if (dom.resultOptimalCost) {
      dom.resultOptimalCost.textContent = formatDollarAmount(outcome.optimalAmount);
    }

    if (dom.resultGap) {
      dom.resultGap.textContent = outcome.gapPercent;
    }

    if (dom.resultBudget) {
      dom.resultBudget.textContent = formatDollarAmount(outcome.budgetAmount);
    }

    if (dom.resultFleet) {
      dom.resultFleet.textContent = `${outcome.fleetUsedCount} ${outcome.fleetUsedCount === 1 ? "bus" : "buses"}`;
    }

    if (dom.resultStars) {
      dom.resultStars.textContent = starBand(outcome.starsEarned);
    }

    if (dom.resultsSummaryText) {
      dom.resultsSummaryText.textContent = outcome.summary;
    }

    renderBulletList(dom.resultsHighlights, outcome.highlights);
  }

  function populateVictoryOverlay(outcome) {
    if (!dom.gameVictoryOverlay || !outcome) {
      return {
        playerPercent: 0,
        oneStarPercent: 0,
        budgetPercent: 0,
        optimalPercent: 0,
        finalStarsEarned: 0,
      };
    }

    if (dom.gameVictoryTitle) {
      dom.gameVictoryTitle.textContent = `Result: ${outcome.title}`;
    }

    if (dom.gameVictoryPlayerCost) {
      dom.gameVictoryPlayerCost.textContent = formatVictoryPopupDollarAmount(outcome.totalCost);
    }

    if (dom.gameVictoryBudget) {
      dom.gameVictoryBudget.textContent = formatVictoryPopupDollarAmount(outcome.budgetAmount);
    }

    if (dom.gameVictoryOptimal) {
      dom.gameVictoryOptimal.textContent = formatVictoryPopupDollarAmount(outcome.optimalAmount);
    }

    if (dom.gameVictoryThreshold) {
      dom.gameVictoryThreshold.textContent = formatVictoryPopupDollarAmount(outcome.oneStarThresholdAmount);
    }

    if (dom.gameVictoryBusRentalCost) {
      dom.gameVictoryBusRentalCost.textContent = formatVictoryPopupDollarAmount(outcome.busRentalCost);
    }

    if (dom.gameVictoryOpportunityCost) {
      dom.gameVictoryOpportunityCost.textContent = formatVictoryPopupDollarAmount(outcome.opportunityCost);
    }

    if (dom.gameVictoryTotalCost) {
      dom.gameVictoryTotalCost.textContent = formatVictoryPopupDollarAmount(outcome.totalCost);
    }

    if (dom.gameVictoryMetricYourCost) {
      dom.gameVictoryMetricYourCost.textContent = formatVictoryPopupDollarAmount(outcome.totalCost);
    }

    if (dom.gameVictoryMetricOptimalCost) {
      dom.gameVictoryMetricOptimalCost.textContent = formatVictoryPopupDollarAmount(outcome.optimalAmount);
    }

    if (dom.gameVictoryGap) {
      dom.gameVictoryGap.textContent = formatVictoryPopupPercentAmount(outcome.gapPercent);
    }

    const victoryScale = outcome.victoryScale || buildVictoryScale(outcome.budgetAmount, outcome.optimalAmount);

    const optimalPercent = positionVictoryScaleElement(dom.gameVictoryMarkerOptimal, outcome.optimalAmount, victoryScale);
    const budgetPercent = positionVictoryScaleElement(dom.gameVictoryMarkerBudget, outcome.budgetAmount, victoryScale);
    const oneStarPercent = positionVictoryScaleElement(
      dom.gameVictoryMarkerThreshold,
      outcome.oneStarThresholdAmount,
      victoryScale,
    );
    const playerPercent = calculateVictoryScalePercent(outcome.totalCost, victoryScale);
    prepareVictoryScoreAnimation();

    dom.gameVictoryMarkerThreshold?.classList.toggle("is-achieved", outcome.starsEarned >= 1);
    dom.gameVictoryMarkerBudget?.classList.toggle("is-achieved", outcome.starsEarned >= 2);
    dom.gameVictoryMarkerOptimal?.classList.toggle("is-achieved", outcome.starsEarned >= 3);

    if (dom.gameVictoryNextLevelButton) {
      dom.gameVictoryNextLevelButton.disabled = !outcome.nextLevel;
    }

    return {
      playerPercent,
      oneStarPercent,
      budgetPercent,
      optimalPercent,
      finalStarsEarned: outcome.starsEarned,
    };
  }

  function hideVictoryOverlay() {
    cancelVictoryScoreAnimation();

    if (state.victoryOverlayTimeoutId) {
      window.clearTimeout(state.victoryOverlayTimeoutId);
      state.victoryOverlayTimeoutId = 0;
    }

    state.isVictoryOverlayVisible = false;

    if (!dom.gameVictoryOverlay) {
      return;
    }

    dom.gameVictoryOverlay.classList.remove("is-visible");
    dom.gameVictoryOverlay.hidden = true;
  }

  function showVictoryOverlay() {
    const outcome = buildCurrentLevelOutcome();

    applyOutcomeToResultsPanels(outcome);
    const scoreAnimation = populateVictoryOverlay(outcome);

    if (!dom.gameVictoryOverlay) {
      return;
    }

    state.isVictoryOverlayVisible = true;
    dom.gameVictoryOverlay.hidden = false;
    window.requestAnimationFrame(() => {
      dom.gameVictoryOverlay?.classList.add("is-visible");
      runVictoryScoreAnimation(scoreAnimation);
    });
  }

  async function runFoundryReplayPlanFromVictoryOverlay(replayPlan, unavailableMessage) {
    if (state.isFoundrySolutionReplayActive || !dom.gameVictoryOverlay || dom.gameVictoryOverlay.hidden) {
      return;
    }

    if (!replayPlan.replayBuses.length) {
      showToast(unavailableMessage, {
        title: "Replay Unavailable",
      });
      return;
    }

    state.isFoundrySolutionReplayActive = true;
    state.isVictoryOverlayVisible = false;
    dom.gameScreen?.classList.add("is-solution-replay-active");

    if (dom.gameVictoryViewSolutionReplayButton) {
      dom.gameVictoryViewSolutionReplayButton.disabled = true;
    }

    if (dom.gameVictoryViewOptimalReplayButton) {
      dom.gameVictoryViewOptimalReplayButton.disabled = true;
    }

    dom.gameVictoryOverlay.classList.remove("is-visible");

    try {
      if (!window.matchMedia?.("(prefers-reduced-motion: reduce)")?.matches) {
        await waitForMs(VICTORY_OVERLAY_FADE_DURATION_MS);
      }

      await playFoundrySolutionReplay(replayPlan);

      if (!state.isFoundrySolutionReplayActive || !dom.gameVictoryOverlay || dom.gameVictoryOverlay.hidden) {
        return;
      }

      window.requestAnimationFrame(() => {
        dom.gameVictoryOverlay?.classList.add("is-visible");
      });
      state.isVictoryOverlayVisible = true;
    } finally {
      stopFoundrySolutionReplay();

      if (dom.gameVictoryViewSolutionReplayButton) {
        dom.gameVictoryViewSolutionReplayButton.disabled = false;
      }

      if (dom.gameVictoryViewOptimalReplayButton) {
        dom.gameVictoryViewOptimalReplayButton.disabled = false;
      }
    }
  }

  async function runFoundrySolutionReplayFromVictoryOverlay() {
    const replayPlan = buildFoundrySolutionReplayPlan();
    await runFoundryReplayPlanFromVictoryOverlay(
      replayPlan,
      "There are no dispatched routes saved for this solution yet.",
    );
  }

  async function runFoundryOptimalReplayFromVictoryOverlay() {
    const replayPlan = buildFoundryOptimalReplayPlan();
    await runFoundryReplayPlanFromVictoryOverlay(
      replayPlan,
      "There is no saved optimal schedule replay for this level yet.",
    );
  }

  function replayVictoryOverlayAnimations() {
    if (!state.isVictoryOverlayVisible || !dom.gameVictoryOverlay || dom.gameVictoryOverlay.hidden) {
      return;
    }

    const outcome = buildCurrentLevelOutcome();
    const scoreAnimation = populateVictoryOverlay(outcome);
    runVictoryScoreAnimation(scoreAnimation);
  }

  function scheduleVictoryOverlay(delayMs = 1200) {
    if (state.isVictoryOverlayVisible || state.victoryOverlayTimeoutId) {
      return;
    }

    state.victoryOverlayTimeoutId = window.setTimeout(() => {
      state.victoryOverlayTimeoutId = 0;
      showVictoryOverlay();
    }, delayMs);
  }

  function canFleetBusAppendRouteNode(bus, nextLabel) {
    if (!bus) {
      return false;
    }

    const projectedRouteMinutes = getFleetBusProjectedRouteMinutes(bus, nextLabel);
    const committedDutyTimeMinutes = Number.isFinite(bus.committedDutyTimeMinutes) ? bus.committedDutyTimeMinutes : 0;
    const projectedDutyTimeMinutes = committedDutyTimeMinutes + projectedRouteMinutes;

    return projectedRouteMinutes <= bus.routeLimitMinutes && projectedDutyTimeMinutes <= bus.dutyLimitMinutes;
  }

  function shouldWarnFoundryRouteLimit(bus, nextLabel) {
    if (!bus || !nextLabel || isFoundryRouteAnimationActive()) {
      return false;
    }

    const routeNodeLabels = getFleetBusRouteNodeLabels(bus);

    if (!routeNodeLabels.length) {
      return false;
    }

    if (getReachableFoundryRouteLabels(bus).includes(nextLabel)) {
      return false;
    }

    const currentLabel = routeNodeLabels[routeNodeLabels.length - 1];
    const previousLabel = routeNodeLabels.length > 1 ? routeNodeLabels[routeNodeLabels.length - 2] : "";
    const forbiddenBrownLabel = getMostRecentFoundryBrownLabel(routeNodeLabels);
    const adjacency = getFoundryRouteAdjacency();
    const nextIsReachableCandidate = getFoundryExtendedRouteCandidates(currentLabel, adjacency).includes(String(nextLabel));

    if (
      !nextIsReachableCandidate
      || String(nextLabel) === previousLabel
      || String(nextLabel) === forbiddenBrownLabel
      || !isSelectableFoundryRouteLabel(nextLabel)
    ) {
      return false;
    }

    return !canFleetBusAppendRouteNode(bus, nextLabel);
  }

  function refreshFleetBusTiming(bus) {
    if (!bus) {
      return;
    }

    const committedDutyTimeMinutes = Number.isFinite(bus.committedDutyTimeMinutes) ? bus.committedDutyTimeMinutes : 0;
    bus.routeTimeMinutes = getFleetBusDraftRouteMinutes(bus);
    bus.dutyTimeMinutes = committedDutyTimeMinutes + bus.routeTimeMinutes;
  }

  function createFoundryStopDemandSnapshot() {
    const demandEntries = Object.entries(getFoundryLevelStopBaseDemands()).map(([routeLabel, count]) => [
      routeLabel,
      Math.max(Number(count) || 0, 0),
    ]);

    state.foundryCustomNodes.forEach((nodeRecord) => {
      if (nodeRecord.variant !== "blue") {
        return;
      }

      demandEntries.push([
        String(nodeRecord.label),
        Math.max(Number(nodeRecord.demand) || 0, 0),
      ]);
    });

    return Object.fromEntries(demandEntries);
  }

  function mergeFoundryStopCounts(targetCounts, sourceCounts) {
    Object.entries(sourceCounts || {}).forEach(([assetKey, count]) => {
      const nextCount = Math.max(Number(count) || 0, 0);

      if (!nextCount) {
        return;
      }

      targetCounts[assetKey] = (targetCounts[assetKey] || 0) + nextCount;
    });

    return targetCounts;
  }

  function renderFoundryStopDemandState(stopDemandState = state.foundryReplayStopDemand || state.foundryStopDemand) {
    dom.foundryPickupNodes?.forEach((pickupNode) => {
      const routeLabel = pickupNode.dataset.routeNodeLabel || "";
      const demandBadge = pickupNode.querySelector(".lmsa-stop-demand");
      const demandCountElement = demandBadge?.querySelector(".lmsa-stop-demand__count");
      const remainingDemand = Math.max(Number(stopDemandState?.[routeLabel]) || 0, 0);

      if (demandCountElement) {
        demandCountElement.textContent = `x${remainingDemand}`;
      }

      if (demandBadge) {
        demandBadge.setAttribute(
          "aria-label",
          remainingDemand === 1 ? "1 worker waiting" : `${remainingDemand} workers waiting`,
        );
      }

      pickupNode.setAttribute(
        "aria-label",
        remainingDemand === 1
          ? "Blue demand node with 1 worker waiting"
          : `Blue demand node with ${remainingDemand} workers waiting`,
      );
    });

    state.foundryCustomNodes.forEach((nodeRecord) => {
      if (nodeRecord.variant !== "blue") {
        return;
      }

      const nodeElement = getFoundryNodeByLabel(nodeRecord.label);
      const demandBadge = nodeElement?.querySelector(".lmsa-stop-demand");
      const demandCountElement = demandBadge?.querySelector(".lmsa-stop-demand__count");
      const remainingDemand = Math.max(Number(stopDemandState?.[nodeRecord.label]) || 0, 0);

      if (!nodeElement || !demandBadge || !demandCountElement) {
        return;
      }

      nodeElement.dataset.builderNodeDemand = String(Math.max(Math.round(Number(nodeRecord.demand) || 0), 0));
      demandCountElement.textContent = `x${remainingDemand}`;
      demandBadge.setAttribute(
        "aria-label",
        remainingDemand === 1 ? "1 worker waiting" : `${remainingDemand} workers waiting`,
      );
      nodeElement.setAttribute(
        "aria-label",
        remainingDemand === 1
          ? "Blue demand node with 1 worker waiting"
          : `Blue demand node with ${remainingDemand} workers waiting`,
      );
    });
  }

  function beginFoundryReplayStopDemandVisualization() {
    state.foundryReplayStopDemand = createFoundryStopDemandSnapshot();
    state.foundryReplayAppliedPickupKeys = new Set();
    renderFoundryStopDemandState(state.foundryReplayStopDemand);
  }

  function applyFoundryReplayPickupVisualization(pickupKey, demandLabel, boardedWorkers) {
    if (
      !pickupKey ||
      !demandLabel ||
      !state.foundryReplayStopDemand ||
      state.foundryReplayAppliedPickupKeys.has(pickupKey)
    ) {
      return;
    }

    const boardedCount = Math.max(Number(boardedWorkers) || 0, 0);

    if (!boardedCount || !(demandLabel in state.foundryReplayStopDemand)) {
      return;
    }

    state.foundryReplayAppliedPickupKeys.add(pickupKey);
    state.foundryReplayStopDemand[demandLabel] = Math.max(
      (Number(state.foundryReplayStopDemand[demandLabel]) || 0) - boardedCount,
      0,
    );
    renderFoundryStopDemandState(state.foundryReplayStopDemand);
  }

  function endFoundryReplayStopDemandVisualization() {
    state.foundryReplayStopDemand = null;
    state.foundryReplayAppliedPickupKeys = new Set();
    renderFoundryStopDemandState();
  }

  function recalculateFoundryTransitState() {
    const remainingDemand = createFoundryStopDemandSnapshot();

    state.purchasedFleet.forEach((bus) => {
      bus.load = 0;
      bus.draftStopCounts = {};
      bus.draftPickupPlan = {};
    });

    state.purchasedFleet.forEach((bus) => {
      Object.entries(bus.servedStopCounts || {}).forEach(([assetKey, count]) => {
        if (!(assetKey in remainingDemand)) {
          return;
        }

        remainingDemand[assetKey] = Math.max(remainingDemand[assetKey] - (Number(count) || 0), 0);
      });
    });

    state.purchasedFleet.forEach((bus) => {
      const draftStopCounts = {};
      const draftPickupPlan = {};
      let draftLoad = 0;

      getFleetBusRouteNodeLabels(bus).forEach((label, routeIndex) => {
        const demandNodeRecord = getFoundryDemandNodeRecord(label);

        if (!demandNodeRecord) {
          return;
        }

        const demandKey = String(demandNodeRecord.label || label);
        const availableDemand = Math.max(Number(remainingDemand[demandKey]) || 0, 0);
        const remainingCapacity = Math.max((Number(bus.capacity) || 0) - draftLoad, 0);
        const boardedWorkers = Math.min(availableDemand, remainingCapacity);

        if (!boardedWorkers) {
          return;
        }

        draftLoad += boardedWorkers;
        remainingDemand[demandKey] -= boardedWorkers;
        draftStopCounts[demandKey] = (draftStopCounts[demandKey] || 0) + boardedWorkers;
        draftPickupPlan[routeIndex] = boardedWorkers;
      });

      bus.load = draftLoad;
      bus.draftStopCounts = draftStopCounts;
      bus.draftPickupPlan = draftPickupPlan;
    });

    state.foundryStopDemand = remainingDemand;
    renderFoundryStopDemandState();

    if (dom.statRemainingDemand) {
      const remainingWorkerCount = Object.values(remainingDemand).reduce(
        (sum, count) => sum + Math.max(Number(count) || 0, 0),
        0,
      );
      dom.statRemainingDemand.textContent = `${remainingWorkerCount} riders`;
    }
  }

  function getFoundryCommittedRemainingDemandCount() {
    const remainingDemand = createFoundryStopDemandSnapshot();

    state.purchasedFleet.forEach((bus) => {
      Object.entries(bus.servedStopCounts || {}).forEach(([assetKey, count]) => {
        if (!(assetKey in remainingDemand)) {
          return;
        }

        remainingDemand[assetKey] = Math.max(remainingDemand[assetKey] - (Number(count) || 0), 0);
      });
    });

    return Object.values(remainingDemand).reduce((sum, count) => sum + Math.max(Number(count) || 0, 0), 0);
  }

  function updateSubmitSolutionButtonState() {
    if (!dom.submitSolutionButton) {
      return;
    }

    const isSolutionReady = isSubmitSolutionReady();

    dom.submitSolutionButton.classList.toggle("is-solution-ready", isSolutionReady);
  }

  function isSubmitSolutionReady() {
    return state.debugForceSubmitReady || (getFoundryCommittedRemainingDemandCount() === 0 && !isFoundryRouteAnimationActive());
  }

  function getFoundryFactoryAsset() {
    return dom.foundryCampusAssets?.find((asset) => asset.dataset.campusAsset === "factory") || null;
  }

  function getDefaultFoundryConfettiEmitterPositions() {
    const factoryAsset = getFoundryFactoryAsset();
    const celebrationLayer = dom.foundryCelebrationLayer;
    const fallbackPositions = [
      { left: 38, top: 13 },
      { left: 45, top: 14 },
    ];

    if (!factoryAsset || !celebrationLayer) {
      return fallbackPositions;
    }

    const celebrationRect = celebrationLayer.getBoundingClientRect();
    const factoryRect = factoryAsset.getBoundingClientRect();

    if (!celebrationRect.width || !celebrationRect.height || !factoryRect.width || !factoryRect.height) {
      return fallbackPositions;
    }

    return [
      {
        left: roundPercent(((factoryRect.left - celebrationRect.left + factoryRect.width * 0.405) / celebrationRect.width) * 100),
        top: roundPercent(((factoryRect.top - celebrationRect.top + factoryRect.height * 0.088) / celebrationRect.height) * 100),
      },
      {
        left: roundPercent(((factoryRect.left - celebrationRect.left + factoryRect.width * 0.545) / celebrationRect.width) * 100),
        top: roundPercent(((factoryRect.top - celebrationRect.top + factoryRect.height * 0.102) / celebrationRect.height) * 100),
      },
    ];
  }

  function loadFoundryConfettiEmitterPositions() {
    if (
      Array.isArray(FOUNDRY_FALLBACK_CONFETTI_EMITTER_POSITIONS) &&
      FOUNDRY_FALLBACK_CONFETTI_EMITTER_POSITIONS.length === 2 &&
      FOUNDRY_FALLBACK_CONFETTI_EMITTER_POSITIONS.every(
        (position) => position && Number.isFinite(position.left) && Number.isFinite(position.top),
      )
    ) {
      return FOUNDRY_FALLBACK_CONFETTI_EMITTER_POSITIONS.map((position) => ({
        left: roundPercent(clamp(position.left, 0, 100)),
        top: roundPercent(clamp(position.top, 0, 100)),
      }));
    }

    try {
      const stored = localStorage.getItem(FOUNDRY_CONFETTI_EMITTER_STORAGE_KEY);

      if (stored) {
        const parsed = JSON.parse(stored);

        if (
          Array.isArray(parsed) &&
          parsed.length === 2 &&
          parsed.every((position) => position && Number.isFinite(position.left) && Number.isFinite(position.top))
        ) {
          return parsed.map((position) => ({
            left: roundPercent(clamp(position.left, 0, 100)),
            top: roundPercent(clamp(position.top, 0, 100)),
          }));
        }
      }
    } catch (_) { /* ignore */ }

    return getDefaultFoundryConfettiEmitterPositions();
  }

  function saveFoundryConfettiEmitterPositions() {
    if (!dom.foundryConfettiEmitterHandles?.length) {
      return;
    }

    try {
      const positions = dom.foundryConfettiEmitterHandles.map((handle) => ({
        left: roundPercent(clamp(parseFloat(handle.style.left || "0"), 0, 100)),
        top: roundPercent(clamp(parseFloat(handle.style.top || "0"), 0, 100)),
      }));

      localStorage.setItem(FOUNDRY_CONFETTI_EMITTER_STORAGE_KEY, JSON.stringify(positions));
    } catch (_) { /* ignore */ }
  }

  function renderFoundryConfettiEmitterHandles() {
    if (!dom.foundryConfettiEmitterHandles?.length) {
      return;
    }

    const positions = loadFoundryConfettiEmitterPositions();

    dom.foundryConfettiEmitterHandles.forEach((handle, index) => {
      const position = positions[index] || positions[positions.length - 1] || { left: 0, top: 0 };
      handle.style.left = `${position.left}%`;
      handle.style.top = `${position.top}%`;
    });
  }

  function getFoundryConfettiEmitterPoints(hostRect = null) {
    const celebrationLayer = dom.foundryCelebrationLayer;

    if (!celebrationLayer) {
      return [];
    }

    if (dom.foundryConfettiEmitterHandles?.length && dom.foundryConfettiEmitterHandles.some((handle) => !handle.style.left || !handle.style.top)) {
      renderFoundryConfettiEmitterHandles();
    }

    const celebrationRect = celebrationLayer.getBoundingClientRect();

    if (!celebrationRect.width || !celebrationRect.height) {
      return [];
    }

    const baseRect = hostRect && Number.isFinite(hostRect.left) && Number.isFinite(hostRect.top)
      ? hostRect
      : celebrationRect;

    const emitterPositions = dom.foundryConfettiEmitterHandles?.length
      ? dom.foundryConfettiEmitterHandles.map((handle) => ({
          left: clamp(parseFloat(handle.style.left || "0"), 0, 100),
          top: clamp(parseFloat(handle.style.top || "0"), 0, 100),
        }))
      : loadFoundryConfettiEmitterPositions();

    return emitterPositions.map((position) => ({
      x:
        (celebrationRect.left - baseRect.left) +
        (clamp(Number(position.left) || 0, 0, 100) / 100) * celebrationRect.width,
      y:
        (celebrationRect.top - baseRect.top) +
        (clamp(Number(position.top) || 0, 0, 100) / 100) * celebrationRect.height,
    }));
  }

  function launchFoundryFactoryConfetti() {
    const celebrationLayer = dom.foundryCelebrationLayer;
    const burstHost = dom.globalConfettiHost || dom.foundryCelebrationBurstHost;

    if (!celebrationLayer || !burstHost) {
      return;
    }

    const celebrationRect = celebrationLayer.getBoundingClientRect();
    const burstHostRect = burstHost.getBoundingClientRect();

    if (!celebrationRect.width || !celebrationRect.height || !burstHostRect.width || !burstHostRect.height) {
      return;
    }

    const emitterPositions = getFoundryConfettiEmitterPoints(burstHostRect);

    if (emitterPositions.length < 2) {
      return;
    }

    burstHost.replaceChildren();

    const burst = document.createElement("div");
    burst.className = "factory-confetti-burst";
    burst.setAttribute("aria-hidden", "true");
    const pieceCount = 42;

    for (let index = 0; index < pieceCount; index += 1) {
      const piece = document.createElement("span");
      const emitter = emitterPositions[index % emitterPositions.length];
      const driftX = (Math.random() * 120 - 60).toFixed(2);
      const driftY = (-84 - Math.random() * 58).toFixed(2);
      const rotate = (Math.random() * 300 - 150).toFixed(2);
      const delay = (Math.random() * 140).toFixed(0);
      const duration = (980 + Math.random() * 420).toFixed(0);
      const width = (8 + Math.random() * 6).toFixed(2);
      const height = (10 + Math.random() * 8).toFixed(2);
      const color = index % 2 === 0 ? "#6b231d" : "#fff8ef";

      piece.className = "factory-confetti-piece";
      piece.style.setProperty("--confetti-origin-x", `${emitter.x}px`);
      piece.style.setProperty("--confetti-origin-y", `${emitter.y}px`);
      piece.style.setProperty("--confetti-drift-x", `${driftX}px`);
      piece.style.setProperty("--confetti-drift-y", `${driftY}px`);
      piece.style.setProperty("--confetti-rotate", `${rotate}deg`);
      piece.style.setProperty("--confetti-delay", `${delay}ms`);
      piece.style.setProperty("--confetti-duration", `${duration}ms`);
      piece.style.setProperty("--confetti-width", `${width}px`);
      piece.style.setProperty("--confetti-height", `${height}px`);
      piece.style.setProperty("--confetti-color", color);
      burst.append(piece);
    }

    burstHost.append(burst);

    window.setTimeout(() => {
      burst.remove();
    }, 1900);
  }

  function syncSelectedFoundryNodeLabel() {
    const selectedBus = getSelectedFleetBus();
    const routeNodeLabels = getFleetBusRouteNodeLabels(selectedBus);
    state.selectedFoundryNodeLabel = routeNodeLabels[routeNodeLabels.length - 1] || "";
  }

  function formatDollarAmount(amount) {
    const value = Number(amount || 0);
    const hasCents = Math.abs(value - Math.round(value)) > 0.0001;

    return value.toLocaleString("en-US", {
      style: "currency",
      currency: "USD",
      minimumFractionDigits: hasCents ? 2 : 0,
      maximumFractionDigits: 2,
    });
  }

  function formatVictoryPopupDollarAmount(amount) {
    const value = Math.round(Number(amount) || 0);
    return value.toLocaleString("en-US", {
      style: "currency",
      currency: "USD",
      minimumFractionDigits: 0,
      maximumFractionDigits: 0,
    });
  }

  function formatVictoryPopupPercentAmount(value) {
    const numericValue = Number.parseFloat(String(value ?? "").replace(/[^0-9.-]/g, ""));
    return `${Math.max(Math.round(Number.isFinite(numericValue) ? numericValue : 0), 0)}%`;
  }

  function formatCountLabel(count, singular, plural = `${singular}s`) {
    const safeCount = Math.max(Math.round(Number(count) || 0), 0);
    return `${safeCount} ${safeCount === 1 ? singular : plural}`;
  }

  function formatMinuteLabel(minutes) {
    const safeMinutes = Math.max(Math.round(Number(minutes) || 0), 0);
    return `${safeMinutes} min`;
  }

  function setStatPillText(valueElement, detailElement, valueText, detailText) {
    if (valueElement) {
      valueElement.textContent = valueText;
    }

    if (detailElement) {
      detailElement.textContent = detailText;
    }
  }

  function renderTopBarStats(level = getLevelData(state.currentLevel)) {
    const selectedBus = getSelectedFleetBus();

    setStatPillText(
      dom.statSelectedBus,
      dom.statSelectedBusDetail,
      selectedBus ? formatSelectedBusCode(selectedBus) : "None",
      selectedBus ? getBusShopConfig(selectedBus.type).name : "Choose from fleet",
    );

    if (selectedBus) {
      setStatPillText(
        dom.statLoad,
        dom.statLoadDetail,
        formatCountLabel(selectedBus.load, "rider"),
        `Max Capacity: ${Math.max(Math.round(Number(selectedBus.capacity) || 0), 0)}`,
      );
      setStatPillText(
        dom.statRouteTime,
        dom.statRouteTimeDetail,
        formatMinuteLabel(selectedBus.routeTimeMinutes),
        `Limit: ${formatMinuteLabel(selectedBus.routeLimitMinutes)}`,
      );
      setStatPillText(
        dom.statDutyTime,
        dom.statDutyTimeDetail,
        formatMinuteLabel(selectedBus.dutyTimeMinutes),
        `Limit: ${formatMinuteLabel(selectedBus.dutyLimitMinutes)}`,
      );
    } else {
      setStatPillText(dom.statLoad, dom.statLoadDetail, "0 riders", "Max Capacity:");
      setStatPillText(dom.statRouteTime, dom.statRouteTimeDetail, "0 min", "Limit:");
      setStatPillText(dom.statDutyTime, dom.statDutyTimeDetail, "60 min", "Limit:");
    }

    setStatPillText(
      dom.statTotalCost,
      dom.statTotalCostDetail,
      formatDollarAmount(getFleetTotalCost()),
      `Budget ${level?.benchmark || formatDollarAmount(0)}`,
    );
  }

  function hideToast() {
    if (state.toastTimeoutId) {
      window.clearTimeout(state.toastTimeoutId);
      state.toastTimeoutId = 0;
    }

    const toastRegion = dom.toastRegion;
    const toast = toastRegion?.firstElementChild;

    if (!toastRegion || !toast) {
      if (toastRegion) {
        toastRegion.hidden = true;
      }
      return;
    }

    toast.classList.remove("is-visible");

    window.setTimeout(() => {
      if (toastRegion.firstElementChild === toast) {
        toastRegion.replaceChildren();
        toastRegion.hidden = true;
      }
    }, 180);
  }

  function showToast(message, options = {}) {
    const toastRegion = dom.toastRegion;

    if (!toastRegion || !message) {
      return;
    }

    if (state.toastTimeoutId) {
      window.clearTimeout(state.toastTimeoutId);
      state.toastTimeoutId = 0;
    }

    const title = options.title || "Warning";
    const toast = document.createElement("div");
    toast.className = "toast";
    toast.setAttribute("role", "status");
    toast.innerHTML = `
      <span class="toast__title">${escapeHtml(title)}</span>
      <p class="toast__message">${escapeHtml(message)}</p>
    `;

    toastRegion.hidden = false;
    toastRegion.replaceChildren(toast);

    window.requestAnimationFrame(() => {
      toast.classList.add("is-visible");
    });

    state.toastTimeoutId = window.setTimeout(() => {
      hideToast();
    }, options.durationMs || 2600);
  }

  function getFleetBusSequence(bus) {
    if (typeof bus?.sequence === "number" && Number.isFinite(bus.sequence)) {
      return bus.sequence;
    }

    const match = String(bus?.id || "").match(/-(\d+)$/);
    return match ? Number(match[1]) : 0;
  }

  function getEarliestAvailableFleetSequence(busType) {
    const usedSequences = new Set(
      state.purchasedFleet
        .filter((bus) => bus.type === busType)
        .map((bus) => getFleetBusSequence(bus))
        .filter((sequence) => sequence > 0),
    );

    let nextSequence = 1;

    while (usedSequences.has(nextSequence)) {
      nextSequence += 1;
    }

    return nextSequence;
  }

  function updateSelectedBusStat() {
    const level = getLevelData(state.currentLevel);
    const selectedBus = getSelectedFleetBus();

    recalculateFoundryTransitState();

    if (selectedBus) {
      refreshFleetBusTiming(selectedBus);
    }
    renderTopBarStats(level);

    updateSubmitSolutionButtonState();
  }

  function updateFoundryDeleteCanState() {
    if (!dom.foundryDeleteCan) {
      return;
    }

    if (state.isEditMode) {
      dom.foundryDeleteCan.disabled = false;
      dom.foundryDeleteCan.classList.remove("is-armed");
      dom.foundryDeleteCan.classList.remove("is-builder-armed");
      return;
    }

    const hasSelectedPurchasedBus = Boolean(getSelectedFleetBus()) && !isFoundryRouteAnimationActive();
    dom.foundryDeleteCan.disabled = !hasSelectedPurchasedBus;
    dom.foundryDeleteCan.classList.toggle("is-armed", hasSelectedPurchasedBus);
    dom.foundryDeleteCan.classList.remove("is-builder-armed");
  }

  function updateHeaderNavigation(topSection, screenTitle, options = {}) {
    const headerLabel = options.headerLabel || topSection;
    const headerSubpage = options.headerSubpage || (screenTitle !== topSection ? screenTitle : "");
    const forceShowAllNav = Boolean(options.forceShowAllNav);
    const activeScreenId = options.activeScreenId || "";

    if (dom.headerCurrentPage) {
      dom.headerCurrentPage.textContent = headerLabel;
    }

    if (dom.headerCurrentSubpage) {
      dom.headerCurrentSubpage.textContent = headerSubpage;
      dom.headerCurrentSubpage.hidden = !headerSubpage;
      dom.headerCurrentSubpage.parentElement?.classList.toggle("header-current--level", Boolean(headerSubpage));
    }

    document.querySelectorAll("[data-nav-section]").forEach((button) => {
      const matchesScreen = activeScreenId && button.dataset.navScreen === activeScreenId;
      const matchesSection = button.dataset.navSection === topSection;
      const hideOnGame = activeScreenId === "game" && button.dataset.hideOnGame === "true";
      button.hidden = hideOnGame || (!forceShowAllNav && (matchesScreen || matchesSection));
    });
  }

  function showScreen(screenId, options = {}) {
    dom.screens.forEach((screen) => {
      const isActive = screen.id === screenId;
      screen.classList.toggle("screen--active", isActive);
      screen.hidden = !isActive;
    });

    document.body.classList.toggle("viewing-game", screenId === "game");

    const activeScreen = document.getElementById(screenId);
    let screenTitle = activeScreen?.dataset.screenTitle || GAME_NAME;
    let topSection = activeScreen?.dataset.topSection || screenTitle;
    let headerLabel = topSection;
    let headerSubpage = screenTitle !== topSection ? screenTitle : "";
    let forceShowAllNav = false;

    if (screenId !== "game") {
      hideVictoryOverlay();
    }

    if (screenId === "game") {
      screenTitle = state.currentLevel;
      topSection = state.currentLevel;
      headerLabel = formatLevelHeader(state.currentLevel);
      headerSubpage = "";
      forceShowAllNav = true;
      window.requestAnimationFrame(() => {
        syncFoundryMapLayout(1);
        renderFoundryConfettiEmitterHandles();
      });
    }

    if (screenId === "howto") {
      window.requestAnimationFrame(() => initializeGuideReplayDemo({ autoplay: true }));
    } else {
      stopGuideReplayDemo({ reset: true });
    }

    state.currentScreen = screenId;
    document.title = `${GAME_NAME} | ${screenTitle}`;
    updateHeaderNavigation(topSection, screenTitle, { headerLabel, headerSubpage, forceShowAllNav, activeScreenId: screenId });

    if (!options.suppressScroll) {
      window.scrollTo({ top: 0, behavior: "smooth" });
    }
  }

  function setCurrentLevel(levelName) {
    const level = getLevelData(levelName);
    stopFoundrySolutionReplay();
    hideVictoryOverlay();
    state.currentLevel = level.name;
    state.purchasedFleet = [];
    state.selectedFleetBusId = "";
    state.debugForceSubmitReady = false;
    state.foundryStopDemand = createFoundryStopDemandSnapshot();

    if (dom.resultsSubtitle) {
      dom.resultsSubtitle.textContent = `${level.name} | ${level.results.subtitle}`;
    }

    if (dom.mapPanelTitle) {
      dom.mapPanelTitle.textContent = level.mapTitle;
    }
    renderTopBarStats(level);

    if (dom.statRemainingDemand) {
      dom.statRemainingDemand.textContent = level.stats.remainingDemand;
    }

    if (dom.resultPlayerCost) {
      dom.resultPlayerCost.textContent = level.results.playerCost;
    }

    if (dom.resultOptimalCost) {
      dom.resultOptimalCost.textContent = level.results.optimalCost;
    }

    if (dom.resultGap) {
      dom.resultGap.textContent = level.results.gap;
    }

    if (dom.resultBudget) {
      dom.resultBudget.textContent = level.results.budget;
    }

    if (dom.resultFleet) {
      dom.resultFleet.textContent = level.results.fleet;
    }

    if (dom.resultStars) {
      dom.resultStars.textContent = starBand(level.results.starsEarned);
    }

    if (dom.resultsSummaryText) {
      dom.resultsSummaryText.textContent = level.results.summary;
    }

    loadFoundryLevelMapState();
    renderBulletList(dom.resultsHighlights, level.results.highlights);
    updateSelectedBusStat();
    renderFoundryFleetYard();
    updateFoundryDeleteCanState();
    saveFoundryScheduleReplaySnapshot();
  }

  function launchLevel(levelName) {
    const level = getLevelData(levelName);

    if (isLevelComingSoon(level)) {
      showToast(getLevelLockedToastMessage(level), {
        title: "Coming Soon",
      });
      return;
    }

    setCurrentLevel(level.name);
    showScreen("game");
  }

  function restartCurrentLevel() {
    setCurrentLevel(state.currentLevel);
    showScreen("game");
  }

  function highlightPanel(panelId) {
    const panel = document.getElementById(panelId);

    if (!panel) {
      return;
    }

    document.querySelectorAll(".info-card.is-highlighted").forEach((card) => {
      card.classList.remove("is-highlighted");
    });

    panel.classList.add("is-highlighted");
    panel.scrollIntoView({ behavior: "smooth", block: "center" });

    window.setTimeout(() => {
      panel.classList.remove("is-highlighted");
    }, 1800);
  }

  function loadFoundryNodePositions() {
    if (!dom.foundryMapNodes?.length) {
      return;
    }

    let activePositions = FOUNDRY_FALLBACK_NODE_POSITIONS;

    if (!FOUNDRY_PREFER_CODE_LAYOUTS_OVER_LOCAL_STORAGE) {
      try {
        const stored = localStorage.getItem("routecraft-nodes");
        if (stored) {
          const parsed = JSON.parse(stored);
          if (parsed && typeof parsed === "object" && Object.keys(parsed).length) {
            activePositions = parsed;
          }
        }
      } catch (_) { /* ignore */ }
    }

    dom.foundryMapNodes.forEach((node) => {
      const storedNode = activePositions?.[node.dataset.label];

      if (!storedNode) {
        return;
      }

      if (typeof storedNode.left === "number") {
        node.style.left = `${storedNode.left}%`;
      }

      if (typeof storedNode.top === "number") {
        node.style.top = `${storedNode.top}%`;
      }
    });
  }

  function clamp(value, min, max) {
    return Math.min(Math.max(value, min), max);
  }

  function roundPercent(value) {
    return Math.round(value * 10) / 10;
  }

  function refreshFoundryMapCollections() {
    dom.foundryMapNodes = Array.from(document.querySelectorAll(".map-stage--foundry .map-node"));
    dom.foundryCampusAssets = Array.from(document.querySelectorAll(".map-stage--foundry .lmsa-campus-asset"));
  }

  function cloneFoundrySerializable(value) {
    if (value == null) {
      return value;
    }

    try {
      return JSON.parse(JSON.stringify(value));
    } catch (_) {
      return null;
    }
  }

  function getFoundryEditLayoutStorageKey(levelName = state.currentLevel) {
    return `${FOUNDRY_EDIT_LAYOUT_STORAGE_KEY_PREFIX}${String(levelName || DEFAULT_LEVEL)}`;
  }

  function normalizeFoundryLayoutRoadRecord(record) {
    const from = String(record?.from || "");
    const to = String(record?.to || "");

    if (!from || !to || from === to) {
      return null;
    }

    return { from, to };
  }

  function buildFoundrySnapshotNodePlacements(snapshotNodes, legacyNodeOverrides) {
    const placementMap = new Map();

    Object.entries(FOUNDRY_FALLBACK_NODE_POSITIONS).forEach(([label, placement]) => {
      placementMap.set(String(label), {
        left: roundPercent(clamp(Number(placement?.left) || 0, 0, 100)),
        top: roundPercent(clamp(Number(placement?.top) || 0, 0, 100)),
      });
    });

    (Array.isArray(legacyNodeOverrides) ? legacyNodeOverrides : []).forEach((override) => {
      const label = String(override?.label || "");

      if (!label) {
        return;
      }

      if (override?.removed) {
        placementMap.delete(label);
        return;
      }

      const left = Number(override?.left);
      const top = Number(override?.top);

      if (Number.isFinite(left) && Number.isFinite(top)) {
        placementMap.set(label, {
          left: roundPercent(clamp(left, 0, 100)),
          top: roundPercent(clamp(top, 0, 100)),
        });
      }
    });

    (Array.isArray(snapshotNodes) ? snapshotNodes : []).forEach((nodeRecord) => {
      const label = String(nodeRecord?.label || "");
      const left = Number(nodeRecord?.left);
      const top = Number(nodeRecord?.top);

      if (!label || !Number.isFinite(left) || !Number.isFinite(top)) {
        return;
      }

      placementMap.set(label, {
        left: roundPercent(clamp(left, 0, 100)),
        top: roundPercent(clamp(top, 0, 100)),
      });
    });

    return placementMap;
  }

  function resolveFoundrySnapshotPickupPlacement(pickupLabel, snapshotRoads, nodePlacements) {
    const pickupConfig = getFoundryLegacyPickupConfig(pickupLabel);

    if (!pickupConfig) {
      return null;
    }

    const anchorPlacement = {
      left: roundPercent(clamp(Number(pickupConfig.anchorLeft) || 0, 0, 100)),
      top: roundPercent(clamp(Number(pickupConfig.anchorTop) || 0, 0, 100)),
    };
    const pickupNeighbors = Array.from(new Set(
      (Array.isArray(snapshotRoads) ? snapshotRoads : []).reduce((neighbors, road) => {
        if (String(road?.from || "") === String(pickupLabel || "") && nodePlacements.has(String(road?.to || ""))) {
          neighbors.push(String(road.to));
        } else if (String(road?.to || "") === String(pickupLabel || "") && nodePlacements.has(String(road?.from || ""))) {
          neighbors.push(String(road.from));
        }

        return neighbors;
      }, []),
    ));
    const candidateSegments = [];

    if (pickupNeighbors.length >= 2) {
      for (let firstIndex = 0; firstIndex < pickupNeighbors.length - 1; firstIndex += 1) {
        for (let secondIndex = firstIndex + 1; secondIndex < pickupNeighbors.length; secondIndex += 1) {
          candidateSegments.push([pickupNeighbors[firstIndex], pickupNeighbors[secondIndex]]);
        }
      }
    } else {
      (Array.isArray(snapshotRoads) ? snapshotRoads : []).forEach((road) => {
        const from = String(road?.from || "");
        const to = String(road?.to || "");

        if (
          !from
          || !to
          || from === String(pickupLabel || "")
          || to === String(pickupLabel || "")
          || !nodePlacements.has(from)
          || !nodePlacements.has(to)
        ) {
          return;
        }

        candidateSegments.push([from, to]);
      });
    }

    let bestPlacement = null;

    candidateSegments.forEach(([from, to]) => {
      const startPlacement = nodePlacements.get(String(from));
      const endPlacement = nodePlacements.get(String(to));

      if (!startPlacement || !endPlacement) {
        return;
      }

      const projectedPlacement = projectPointOntoFoundryPercentSegment(anchorPlacement, startPlacement, endPlacement);
      const distance = Math.hypot(
        anchorPlacement.left - projectedPlacement.left,
        anchorPlacement.top - projectedPlacement.top,
      );

      if (!bestPlacement || distance < bestPlacement.distance) {
        bestPlacement = {
          left: roundPercent(clamp(projectedPlacement.left, 0, 100)),
          top: roundPercent(clamp(projectedPlacement.top, 0, 100)),
          distance,
        };
      }
    });

    return bestPlacement
      ? { left: bestPlacement.left, top: bestPlacement.top }
      : anchorPlacement;
  }

  function normalizeFoundryEditLayoutSnapshot(snapshot, levelName = state.currentLevel) {
    const safeLevelName = String(levelName || DEFAULT_LEVEL);
    const normalizedSnapshot = cloneFoundrySerializable(snapshot);

    if (!normalizedSnapshot || typeof normalizedSnapshot !== "object") {
      return null;
    }

    const normalizedNodes = Array.isArray(normalizedSnapshot.nodes)
      ? normalizedSnapshot.nodes.map((record) => ({
        ...record,
        label: String(record?.label || ""),
      }))
      : [];
    const normalizedRoads = Array.isArray(normalizedSnapshot.roads)
      ? normalizedSnapshot.roads
          .map(normalizeFoundryLayoutRoadRecord)
          .filter(Boolean)
      : [];
    const legacyNodeOverrides = Array.isArray(normalizedSnapshot.legacyNodeOverrides)
      ? normalizedSnapshot.legacyNodeOverrides
      : [];
    const migratedPickupLabelMap = new Map();

    if (safeLevelName !== DEFAULT_LEVEL) {
      const pickupLabelsToMigrate = new Set();

      normalizedNodes.forEach((nodeRecord) => {
        const label = String(nodeRecord?.label || "");

        if (getFoundryLegacyPickupConfig(label)) {
          pickupLabelsToMigrate.add(label);
        }
      });

      normalizedRoads.forEach((road) => {
        [road.from, road.to].forEach((label) => {
          if (getFoundryLegacyPickupConfig(label)) {
            pickupLabelsToMigrate.add(label);
          }
        });
      });

      if (pickupLabelsToMigrate.size) {
        const nodePlacements = buildFoundrySnapshotNodePlacements(normalizedNodes, legacyNodeOverrides);
        let nextNodeLabel = normalizedNodes.reduce((highestLabel, nodeRecord) => {
          const numericLabel = Number(nodeRecord?.label);
          return Number.isFinite(numericLabel) ? Math.max(highestLabel, numericLabel) : highestLabel;
        }, FOUNDRY_CUSTOM_NODE_FIRST_LABEL - 1);

        pickupLabelsToMigrate.forEach((pickupLabel) => {
          const existingPickupNodeRecord = normalizedNodes.find(
            (nodeRecord) => String(nodeRecord?.label || "") === String(pickupLabel),
          );
          const resolvedPlacement =
            resolveFoundrySnapshotPickupPlacement(pickupLabel, normalizedRoads, nodePlacements)
            || {
              left: roundPercent(clamp(Number(getFoundryLegacyPickupConfig(pickupLabel)?.anchorLeft) || 0, 0, 100)),
              top: roundPercent(clamp(Number(getFoundryLegacyPickupConfig(pickupLabel)?.anchorTop) || 0, 0, 100)),
            };
          const migratedLabel = String(nextNodeLabel + 1);
          const migratedDemand = existingPickupNodeRecord?.variant === "blue"
            ? Math.max(Math.round(Number(existingPickupNodeRecord?.demand) || getFoundryLegacyPickupDemand(pickupLabel)), 0)
            : getFoundryLegacyPickupDemand(pickupLabel);

          nextNodeLabel += 1;
          migratedPickupLabelMap.set(String(pickupLabel), migratedLabel);
          normalizedNodes.push({
            label: migratedLabel,
            variant: "blue",
            left: resolvedPlacement.left,
            top: resolvedPlacement.top,
            demand: migratedDemand,
          });
          nodePlacements.set(migratedLabel, resolvedPlacement);
        });
      }
    }

    normalizedSnapshot.nodes = normalizedNodes.filter((nodeRecord) => {
      return !migratedPickupLabelMap.has(String(nodeRecord?.label || ""));
    });
    normalizedSnapshot.roads = normalizedRoads.map((road) => ({
      from: migratedPickupLabelMap.get(String(road.from)) || String(road.from),
      to: migratedPickupLabelMap.get(String(road.to)) || String(road.to),
    }));
    normalizedSnapshot.assets = [];
    normalizedSnapshot.legacyAssetOverrides = [];

    return normalizedSnapshot;
  }

  function readFoundryEditLayoutSnapshot(levelName = state.currentLevel) {
    const storageKey = getFoundryEditLayoutStorageKey(levelName);
    let storedLayout = cloneFoundrySerializable(FOUNDRY_SAVED_EDIT_LAYOUT_FALLBACKS[levelName]) || null;

    if (!storedLayout && !FOUNDRY_PREFER_CODE_LAYOUTS_OVER_LOCAL_STORAGE) {
      try {
        const stored = localStorage.getItem(storageKey);
        if (stored) {
          storedLayout = JSON.parse(stored);
        }
      } catch (_) { /* ignore */ }
    }

    if (!storedLayout && !FOUNDRY_PREFER_CODE_LAYOUTS_OVER_LOCAL_STORAGE) {
      try {
        const legacyStored = localStorage.getItem(FOUNDRY_LEGACY_EDIT_LAYOUT_STORAGE_KEY);
        if (legacyStored) {
          const parsedLegacyLayout = JSON.parse(legacyStored);
          const legacyLevelName = String(parsedLegacyLayout?.levelName || "");

          if (legacyLevelName === String(levelName)) {
            storedLayout = parsedLegacyLayout;
          }
        }
      } catch (_) { /* ignore */ }
    }

    return normalizeFoundryEditLayoutSnapshot(storedLayout, levelName);
  }

  function buildFoundryEditLayoutSnapshot() {
    return {
      version: 1,
      levelName: state.currentLevel,
      capturedAt: Date.now(),
      nodes: state.foundryCustomNodes.map((node) => ({
        label: String(node.label),
        variant: node.variant === "blue" ? "blue" : "brown",
        left: roundPercent(Number(node.left) || 0),
        top: roundPercent(Number(node.top) || 0),
        demand: node.variant === "blue" ? Math.max(Math.round(Number(node.demand) || 0), 0) : 0,
      })),
      assets: [],
      roads: state.foundryRoads.map((road) => ({
        from: String(road.from),
        to: String(road.to),
      })),
      legacyNodeOverrides: state.foundryLegacyNodeOverrides.map((override) => ({
        label: String(override.label),
        left: override.left == null ? null : roundPercent(Number(override.left) || 0),
        top: override.top == null ? null : roundPercent(Number(override.top) || 0),
        removed: Boolean(override.removed),
      })),
      legacyAssetOverrides: [],
    };
  }

  function saveFoundryEditLayoutSnapshot() {
    const snapshot = buildFoundryEditLayoutSnapshot();
    state.foundryLoadedEditLayout = cloneFoundrySerializable(snapshot);

    try {
      localStorage.setItem(getFoundryEditLayoutStorageKey(snapshot.levelName), JSON.stringify(snapshot));
    } catch (_) { /* ignore */ }
  }

  function sanitizeFoundryBuilderNodeRecord(record) {
    const numericLabel = Math.max(Math.round(Number(record?.label) || 0), FOUNDRY_CUSTOM_NODE_FIRST_LABEL);

    if (!Number.isFinite(numericLabel)) {
      return null;
    }

    return {
      label: String(numericLabel),
      variant: record?.variant === "blue" ? "blue" : "brown",
      left: roundPercent(clamp(Number(record?.left) || 0, 0, 100)),
      top: roundPercent(clamp(Number(record?.top) || 0, 0, 100)),
      demand: record?.variant === "blue"
        ? Math.max(Math.round(Number(record?.demand) || 1), 0)
        : 0,
    };
  }

  function sanitizeFoundryBuilderAssetRecord(record, index = 0) {
    const fallbackId = `builder-stop-sign-${index + 1}`;
    const assetType = String(record?.type || "stop-sign");
    const scale = Number(record?.scale);
    const defaultScale = assetType === "stop-sign" ? FOUNDRY_DEFAULT_BUILDER_STOP_SIGN_SCALE : 1;
    const normalizedScale = Number.isFinite(scale) && scale > 0
      ? Math.max(scale, defaultScale)
      : defaultScale;

    return {
      id: String(record?.id || fallbackId),
      type: assetType,
      left: roundPercent(clamp(Number(record?.left) || 0, 0, 100)),
      top: roundPercent(clamp(Number(record?.top) || 0, 0, 100)),
      rotation: roundPercent(Number(record?.rotation) || 0),
      scale: roundPercent(normalizedScale),
      demand: Math.max(Math.round(Number(record?.demand) || 0), 0),
      pairedNodeLabel: record?.pairedNodeLabel ? String(record.pairedNodeLabel) : "",
    };
  }

  function sanitizeFoundryLegacyNodeOverride(record) {
    const label = String(record?.label || "");

    if (!label) {
      return null;
    }

    const leftValue = Number(record?.left);
    const topValue = Number(record?.top);

    return {
      label,
      left: Number.isFinite(leftValue) ? roundPercent(clamp(leftValue, 0, 100)) : null,
      top: Number.isFinite(topValue) ? roundPercent(clamp(topValue, 0, 100)) : null,
      removed: Boolean(record?.removed),
    };
  }

  function sanitizeFoundryLegacyAssetOverride(record) {
    const id = String(record?.id || "");

    if (!id) {
      return null;
    }

    const leftValue = Number(record?.left);
    const topValue = Number(record?.top);
    const rotationValue = Number(record?.rotation);
    const scaleValue = Number(record?.scale);

    return {
      id,
      left: Number.isFinite(leftValue) ? roundPercent(clamp(leftValue, 0, 100)) : null,
      top: Number.isFinite(topValue) ? roundPercent(clamp(topValue, 0, 100)) : null,
      rotation: Number.isFinite(rotationValue) ? roundPercent(rotationValue) : null,
      scale: Number.isFinite(scaleValue) && scaleValue > 0 ? roundPercent(scaleValue) : null,
      removed: Boolean(record?.removed),
    };
  }

  function getNextFoundryCustomNodeLabel() {
    const maxExistingLabel = dom.foundryMapNodes.reduce((highestLabel, node) => {
      const nodeLabel = Number(node?.dataset?.label);
      return Number.isFinite(nodeLabel) ? Math.max(highestLabel, nodeLabel) : highestLabel;
    }, FOUNDRY_CUSTOM_NODE_FIRST_LABEL - 1);

    return String(maxExistingLabel + 1);
  }

  function getNextFoundryCustomAssetId(type = "stop-sign") {
    const safeType = String(type || "asset");
    const prefix = `builder-${safeType}`;
    const maxExistingId = state.foundryCustomAssets.reduce((highestId, asset) => {
      const match = String(asset?.id || "").match(new RegExp(`^${prefix}-(\\d+)$`));
      const numericId = match ? Number(match[1]) : 0;
      return Number.isFinite(numericId) ? Math.max(highestId, numericId) : highestId;
    }, 0);

    return `${prefix}-${maxExistingId + 1}`;
  }

  function createFoundryBuilderNodeElement(record) {
    const node = document.createElement("span");
    node.className = `map-node map-node--builder${record.variant === "blue" ? " map-node--blue" : ""}`;
    node.dataset.label = String(record.label);
    node.dataset.nodeRole = "interior";
    node.dataset.builderNode = "true";
    node.dataset.builderNodeVariant = record.variant === "blue" ? "blue" : "brown";
    node.dataset.builderNodeDemand = String(Math.max(Math.round(Number(record.demand) || 0), 0));
    node.style.left = `${record.left}%`;
    node.style.top = `${record.top}%`;

    if (record.variant === "blue") {
      node.innerHTML = `
        <span class="lmsa-stop-demand" aria-label="1 worker waiting">
          <span class="lmsa-stop-demand__icon">
            <img class="lmsa-stop-demand__icon-image" src="LMSA%20Worker%20Head.png" alt="" draggable="false">
          </span>
          <span class="lmsa-stop-demand__count">x${Math.max(Math.round(Number(record.demand) || 0), 0)}</span>
        </span>
      `;
      node.setAttribute("aria-label", "Blue demand node");
    }

    return node;
  }

  function createFoundryBuilderAssetElement(record) {
    const asset = document.createElement("div");
    asset.className = "lmsa-campus-asset lmsa-campus-asset--stop-sign lmsa-campus-asset--builder";
    asset.dataset.campusAsset = String(record.id);
    asset.dataset.builderAsset = "true";
    asset.dataset.builderAssetType = String(record.type || "stop-sign");
    asset.dataset.builderDemand = String(Math.max(Math.round(Number(record.demand) || 0), 0));
    asset.setAttribute("aria-label", "LMSA bus stop sign");
    asset.innerHTML = `
      <div class="lmsa-stop-demand" aria-label="0 workers waiting" hidden>
        <span class="lmsa-stop-demand__icon">
          <img class="lmsa-stop-demand__icon-image" src="LMSA%20Worker%20Head.png" alt="" draggable="false">
        </span>
        <span class="lmsa-stop-demand__count">x0</span>
      </div>
      <img class="lmsa-campus-asset__image" src="LMSA%20Bus%20Stop%20Sign.png" alt="LMSA bus stop sign" draggable="false">
    `;
    applyFoundryCampusAssetPlacement(asset, record);
    return asset;
  }

  function appendFoundryBuilderNode(record) {
    if (!dom.foundryMapStage) {
      return null;
    }

    const node = createFoundryBuilderNodeElement(record);
    dom.foundryMapStage.insertBefore(node, dom.gameVictoryOverlay || null);
    refreshFoundryMapCollections();
    return node;
  }

  function appendFoundryBuilderAsset(record) {
    if (!dom.foundryMapStage) {
      return null;
    }

    const asset = createFoundryBuilderAssetElement(record);
    dom.foundryMapStage.insertBefore(asset, dom.gameVictoryOverlay || null);
    refreshFoundryMapCollections();
    return asset;
  }

  function loadFoundryEditLayout() {
    if (!dom.foundryMapStage) {
      return;
    }

    dom.foundryMapStage.querySelectorAll("[data-builder-node=\"true\"], [data-builder-asset=\"true\"]").forEach((element) => {
      element.remove();
    });

    state.foundryCustomNodes = [];
    state.foundryCustomAssets = [];
    state.foundryLegacyNodeOverrides = [];
    state.foundryLegacyAssetOverrides = [];
    state.foundryLoadedEditLayout = null;
    refreshFoundryMapCollections();

    const storedLayout = readFoundryEditLayoutSnapshot(state.currentLevel);
    state.foundryLoadedEditLayout = cloneFoundrySerializable(storedLayout);

    const customNodes = Array.isArray(storedLayout?.nodes) ? storedLayout.nodes : [];
    const legacyNodeOverrides = Array.isArray(storedLayout?.legacyNodeOverrides) ? storedLayout.legacyNodeOverrides : [];
    const seenLabels = new Set();
    const seenLegacyNodeLabels = new Set();

    state.foundryCustomNodes = customNodes.reduce((nodes, record) => {
      const sanitizedRecord = sanitizeFoundryBuilderNodeRecord(record);

      if (!sanitizedRecord || seenLabels.has(sanitizedRecord.label)) {
        return nodes;
      }

      seenLabels.add(sanitizedRecord.label);
      nodes.push(sanitizedRecord);
      appendFoundryBuilderNode(sanitizedRecord);
      return nodes;
    }, []);

    state.foundryCustomAssets = [];

    state.foundryLegacyNodeOverrides = legacyNodeOverrides.reduce((overrides, record) => {
      const sanitizedRecord = sanitizeFoundryLegacyNodeOverride(record);

      if (!sanitizedRecord || seenLegacyNodeLabels.has(sanitizedRecord.label)) {
        return overrides;
      }

      seenLegacyNodeLabels.add(sanitizedRecord.label);
      overrides.push(sanitizedRecord);
      return overrides;
    }, []);

    state.foundryLegacyAssetOverrides = [];
  }

  function addFoundryBuilderNode(variant, placement) {
    const nodeRecord = sanitizeFoundryBuilderNodeRecord({
      label: getNextFoundryCustomNodeLabel(),
      variant,
      left: placement?.left,
      top: placement?.top,
    });

    if (!nodeRecord) {
      return null;
    }

    state.foundryCustomNodes.push(nodeRecord);
    return {
      record: nodeRecord,
      element: appendFoundryBuilderNode(nodeRecord),
    };
  }

  function addFoundryBuilderAsset(type, placement) {
    const assetRecord = sanitizeFoundryBuilderAssetRecord({
      id: getNextFoundryCustomAssetId(type),
      type,
      left: placement?.left,
      top: placement?.top,
      rotation: 0,
      scale: type === "stop-sign" ? FOUNDRY_DEFAULT_BUILDER_STOP_SIGN_SCALE : 1,
      demand: 0,
    });

    state.foundryCustomAssets.push(assetRecord);
    return {
      record: assetRecord,
      element: appendFoundryBuilderAsset(assetRecord),
    };
  }

  function addFoundryBuilderStopPair(placement) {
    const assetItem = addFoundryBuilderAsset("stop-sign", placement);

    if (!assetItem?.record) {
      return null;
    }

    const nodeItem = addFoundryBuilderNode("blue", placement);

    if (!nodeItem?.record) {
      return assetItem;
    }

    assetItem.record.pairedNodeLabel = String(nodeItem.record.label);
    syncFoundryBuilderNodeElement(nodeItem.record);

    return {
      asset: assetItem,
      node: nodeItem,
    };
  }

  function syncFoundryBuilderPairedBlueNodeToAsset(assetRecord) {
    if (!assetRecord?.pairedNodeLabel) {
      return null;
    }

    const nodeRecord = state.foundryCustomNodes.find(
      (node) => node.variant === "blue" && node.label === String(assetRecord.pairedNodeLabel),
    );

    if (!nodeRecord) {
      return null;
    }

    nodeRecord.left = roundPercent(clamp(Number(assetRecord.left) || 0, 0, 100));
    nodeRecord.top = roundPercent(clamp(Number(assetRecord.top) || 0, 0, 100));
    syncFoundryBuilderNodeElement(nodeRecord);
    return nodeRecord;
  }

  function updateFoundryBuilderNodePosition(label, placement) {
    const nodeRecord = state.foundryCustomNodes.find((node) => node.label === String(label));

    if (!nodeRecord) {
      return null;
    }

    nodeRecord.left = roundPercent(clamp(Number(placement?.left) || 0, 0, 100));
    nodeRecord.top = roundPercent(clamp(Number(placement?.top) || 0, 0, 100));
    return nodeRecord;
  }

  function updateFoundryBuilderAssetPlacement(assetId, placement) {
    const assetRecord = state.foundryCustomAssets.find((asset) => asset.id === String(assetId));

    if (!assetRecord) {
      return null;
    }

    assetRecord.left = roundPercent(clamp(Number(placement?.left) || 0, 0, 100));
    assetRecord.top = roundPercent(clamp(Number(placement?.top) || 0, 0, 100));
    assetRecord.rotation = roundPercent(Number(placement?.rotation) || 0);
    assetRecord.scale = roundPercent(Number(placement?.scale) || FOUNDRY_DEFAULT_BUILDER_STOP_SIGN_SCALE);
    return assetRecord;
  }

  function removeFoundryBuilderNode(label, options) {
    const nodeLabel = String(label);
    const skipPaired = Boolean(options && options.skipPaired);
    const pairedAssetIds = skipPaired
      ? []
      : state.foundryCustomAssets
          .filter((assetRecord) => assetRecord.pairedNodeLabel === nodeLabel)
          .map((assetRecord) => assetRecord.id);

    state.foundryCustomNodes = state.foundryCustomNodes.filter((node) => node.label !== nodeLabel);
    state.foundryRoads = state.foundryRoads.filter((road) => road.from !== nodeLabel && road.to !== nodeLabel);
    state.foundryCustomAssets.forEach((assetRecord) => {
      if (assetRecord.pairedNodeLabel === nodeLabel) {
        assetRecord.pairedNodeLabel = "";
      }
    });
    dom.foundryMapStage?.querySelector(`[data-builder-node="true"][data-label="${nodeLabel}"]`)?.remove();

    if (state.selectedFoundryNodeLabel === nodeLabel) {
      clearFoundryNodeSelection();
    }

    pairedAssetIds.forEach((assetId) => {
      removeFoundryBuilderAsset(assetId, { skipPaired: true });
    });

    refreshFoundryMapCollections();
    updateSelectedBusStat();
  }

  function removeFoundryBuilderAsset(assetId, options) {
    const targetId = String(assetId);
    const skipPaired = Boolean(options && options.skipPaired);
    const pairedNodeLabel = skipPaired
      ? ""
      : state.foundryCustomAssets.find((assetRecord) => assetRecord.id === targetId)?.pairedNodeLabel || "";

    state.foundryCustomAssets = state.foundryCustomAssets.filter((asset) => asset.id !== targetId);
    dom.foundryMapStage?.querySelector(`[data-builder-asset="true"][data-campus-asset="${targetId}"]`)?.remove();

    if (state.selectedFoundryBuilderAssetId === targetId) {
      clearFoundryBuilderAssetSelection();
    }

    if (pairedNodeLabel) {
      removeFoundryBuilderNode(pairedNodeLabel, { skipPaired: true });
    }

    refreshFoundryMapCollections();
  }

  function isLegacyFoundryNodeElement(node) {
    return Boolean(
      node
      && node.classList?.contains("map-node")
      && node.dataset?.builderNode !== "true",
    );
  }

  function isLegacyFoundryAssetElement(asset) {
    if (!asset || !asset.classList?.contains("lmsa-campus-asset") || asset.dataset?.builderAsset === "true") {
      return false;
    }

    const assetKey = asset.dataset?.campusAsset || "";
    return Boolean(assetKey) && assetKey !== "factory";
  }

  function getFoundryLegacyNodeOverride(label) {
    return state.foundryLegacyNodeOverrides.find((override) => override.label === String(label)) || null;
  }

  function getFoundryLegacyAssetOverride(id) {
    return state.foundryLegacyAssetOverrides.find((override) => override.id === String(id)) || null;
  }

  function upsertFoundryLegacyNodeOverride(label, patch) {
    const targetLabel = String(label || "");

    if (!targetLabel) {
      return null;
    }

    let override = getFoundryLegacyNodeOverride(targetLabel);

    if (!override) {
      override = { label: targetLabel, left: null, top: null, removed: false };
      state.foundryLegacyNodeOverrides.push(override);
    }

    if (patch && typeof patch === "object") {
      if ("left" in patch) {
        const leftValue = Number(patch.left);
        override.left = Number.isFinite(leftValue) ? roundPercent(clamp(leftValue, 0, 100)) : null;
      }

      if ("top" in patch) {
        const topValue = Number(patch.top);
        override.top = Number.isFinite(topValue) ? roundPercent(clamp(topValue, 0, 100)) : null;
      }

      if ("removed" in patch) {
        override.removed = Boolean(patch.removed);
      }
    }

    return override;
  }

  function upsertFoundryLegacyAssetOverride(id, patch) {
    const targetId = String(id || "");

    if (!targetId) {
      return null;
    }

    let override = getFoundryLegacyAssetOverride(targetId);

    if (!override) {
      override = { id: targetId, left: null, top: null, rotation: null, scale: null, removed: false };
      state.foundryLegacyAssetOverrides.push(override);
    }

    if (patch && typeof patch === "object") {
      if ("left" in patch) {
        const leftValue = Number(patch.left);
        override.left = Number.isFinite(leftValue) ? roundPercent(clamp(leftValue, 0, 100)) : null;
      }

      if ("top" in patch) {
        const topValue = Number(patch.top);
        override.top = Number.isFinite(topValue) ? roundPercent(clamp(topValue, 0, 100)) : null;
      }

      if ("rotation" in patch) {
        const rotationValue = Number(patch.rotation);
        override.rotation = Number.isFinite(rotationValue) ? roundPercent(rotationValue) : null;
      }

      if ("scale" in patch) {
        const scaleValue = Number(patch.scale);
        override.scale = Number.isFinite(scaleValue) && scaleValue > 0 ? roundPercent(scaleValue) : null;
      }

      if ("removed" in patch) {
        override.removed = Boolean(patch.removed);
      }
    }

    return override;
  }

  function updateLegacyFoundryNodePosition(label, placement) {
    return upsertFoundryLegacyNodeOverride(label, {
      left: Number(placement?.left),
      top: Number(placement?.top),
    });
  }

  function updateLegacyFoundryAssetPlacement(id, placement) {
    return upsertFoundryLegacyAssetOverride(id, {
      left: Number(placement?.left),
      top: Number(placement?.top),
      rotation: Number(placement?.rotation),
      scale: Number(placement?.scale),
    });
  }

  function removeLegacyFoundryNode(label) {
    const nodeLabel = String(label);
    upsertFoundryLegacyNodeOverride(nodeLabel, { removed: true });
    state.foundryRoads = state.foundryRoads.filter((road) => road.from !== nodeLabel && road.to !== nodeLabel);

    const nodeElement = dom.foundryMapStage?.querySelector(`.map-node[data-label="${nodeLabel}"]:not([data-builder-node="true"])`);

    if (nodeElement) {
      nodeElement.hidden = true;
      nodeElement.classList.add("map-node--legacy-removed");
    }

    if (state.selectedFoundryNodeLabel === nodeLabel) {
      clearFoundryNodeSelection();
    }
  }

  function removeLegacyFoundryAsset(id) {
    const assetId = String(id);
    upsertFoundryLegacyAssetOverride(assetId, { removed: true });

    const assetElement = dom.foundryMapStage?.querySelector(`.lmsa-campus-asset[data-campus-asset="${assetId}"]:not([data-builder-asset="true"])`);

    if (assetElement) {
      assetElement.hidden = true;
      assetElement.classList.add("lmsa-campus-asset--legacy-removed");
    }

    dom.foundryPickupNodes?.forEach((pickupNode) => {
      if (pickupNode.dataset.stopAsset === assetId) {
        pickupNode.hidden = true;
      }
    });
  }

  function applyFoundryLegacyOverridesToDom() {
    state.foundryLegacyNodeOverrides.forEach((override) => {
      const nodeElement = dom.foundryMapStage?.querySelector(`.map-node[data-label="${override.label}"]:not([data-builder-node="true"])`);

      if (!nodeElement) {
        return;
      }

      if (override.left != null) {
        nodeElement.style.left = `${override.left}%`;
      }

      if (override.top != null) {
        nodeElement.style.top = `${override.top}%`;
      }

      if (override.removed) {
        nodeElement.hidden = true;
        nodeElement.classList.add("map-node--legacy-removed");
      } else {
        nodeElement.hidden = false;
        nodeElement.classList.remove("map-node--legacy-removed");
      }
    });

    state.foundryLegacyAssetOverrides.forEach((override) => {
      const assetElement = dom.foundryMapStage?.querySelector(`.lmsa-campus-asset[data-campus-asset="${override.id}"]:not([data-builder-asset="true"])`);

      if (assetElement) {
        const existingRotation = Number(assetElement.dataset.rotation) || 0;
        const existingScale = Number(assetElement.dataset.scale) || 1;

        applyFoundryCampusAssetPlacement(assetElement, {
          left: override.left != null ? override.left : parseFloat(assetElement.style.left),
          top: override.top != null ? override.top : parseFloat(assetElement.style.top),
          rotation: override.rotation != null ? override.rotation : existingRotation,
          scale: override.scale != null ? override.scale : existingScale,
        });

        if (override.removed) {
          assetElement.hidden = true;
          assetElement.classList.add("lmsa-campus-asset--legacy-removed");
        } else {
          assetElement.hidden = false;
          assetElement.classList.remove("lmsa-campus-asset--legacy-removed");
        }
      }

      if (override.removed) {
        dom.foundryPickupNodes?.forEach((pickupNode) => {
          if (pickupNode.dataset.stopAsset === override.id) {
            pickupNode.hidden = true;
          }
        });
      }
    });
  }

  function clearFoundryBuilderAssetSelection() {
    if (state.selectedFoundryBuilderAssetId) {
      dom.foundryMapStage?.querySelectorAll(".lmsa-campus-asset--builder-selected").forEach((element) => {
        element.classList.remove("lmsa-campus-asset--builder-selected");
      });
    }

    state.selectedFoundryBuilderAssetId = "";
  }

  function setFoundryBuilderAssetSelection(assetId) {
    const targetId = String(assetId || "");

    if (!targetId) {
      clearFoundryBuilderAssetSelection();
      return;
    }

    clearFoundryBuilderAssetSelection();

    const assetElement = dom.foundryMapStage?.querySelector(`.lmsa-campus-asset[data-builder-asset="true"][data-campus-asset="${targetId}"]`);

    if (!assetElement) {
      return;
    }

    state.selectedFoundryBuilderAssetId = targetId;
    assetElement.classList.add("lmsa-campus-asset--builder-selected");
  }

  function getFoundryNodeByLabel(label) {
    return dom.foundryMapNodes.find((node) => node.dataset.label === String(label)) || null;
  }

  function getFoundryCampusAsset(assetKey) {
    return dom.foundryCampusAssets.find((asset) => asset.dataset.campusAsset === String(assetKey)) || null;
  }

  function getFoundryBuilderNodeRecord(label) {
    return state.foundryCustomNodes.find((node) => node.label === String(label)) || null;
  }

  function getFoundryBuilderAssetRecord(assetId) {
    return state.foundryCustomAssets.find((asset) => asset.id === String(assetId)) || null;
  }

  function getFoundryNodePercentPlacement(label) {
    const node = getFoundryNodeByLabel(label);

    if (!node) {
      return null;
    }

    const left = parseFloat(node.style.left);
    const top = parseFloat(node.style.top);

    if (!Number.isFinite(left) || !Number.isFinite(top)) {
      const stageRect = dom.foundryMapStage?.getBoundingClientRect();

      if (stageRect?.width && stageRect?.height) {
        return measureFoundryNodePercent(node);
      }

      return null;
    }

    return {
      left: roundPercent(left),
      top: roundPercent(top),
    };
  }

  function projectPointOntoFoundryPercentSegment(point, start, end) {
    const deltaLeft = end.left - start.left;
    const deltaTop = end.top - start.top;
    const segmentLengthSquared = deltaLeft ** 2 + deltaTop ** 2;

    if (!segmentLengthSquared) {
      return { left: start.left, top: start.top, t: 0 };
    }

    const projectionT = clamp(
      (((point.left - start.left) * deltaLeft) + ((point.top - start.top) * deltaTop)) / segmentLengthSquared,
      0,
      1,
    );

    return {
      left: start.left + deltaLeft * projectionT,
      top: start.top + deltaTop * projectionT,
      t: projectionT,
    };
  }

  function findNearestFoundryRoadPlacement(placement) {
    if (!placement) {
      return null;
    }

    const stageRect = dom.foundryMapStage?.getBoundingClientRect();

    if (!stageRect?.width || !stageRect?.height) {
      return null;
    }

    const targetPoint = {
      x: (clamp(Number(placement.left) || 0, 0, 100) / 100) * stageRect.width,
      y: (clamp(Number(placement.top) || 0, 0, 100) / 100) * stageRect.height,
    };

    let bestPlacement = null;

    state.foundryRoads.forEach((road) => {
      const fromNode = getFoundryNodeByLabel(road.from);
      const toNode = getFoundryNodeByLabel(road.to);

      if (!fromNode || !toNode || isPerimeterFoundryNode(fromNode) || isPerimeterFoundryNode(toNode)) {
        return;
      }

      const start = measureFoundryNodeCenter(fromNode);
      const end = measureFoundryNodeCenter(toNode);

      if (!start || !end) {
        return;
      }

      const projection = projectPointOntoFoundrySegment(targetPoint, start, end);
      const distance = Math.hypot(targetPoint.x - projection.x, targetPoint.y - projection.y);

      if (!bestPlacement || distance < bestPlacement.distance) {
        bestPlacement = {
          from: String(road.from),
          to: String(road.to),
          distance,
          targetPoint,
          projectionPoint: {
            x: projection.x,
            y: projection.y,
          },
        };
      }
    });

    return bestPlacement;
  }

  function constrainFoundryBuilderBlueNodePlacement(placement) {
    if (!placement) {
      return null;
    }

    const stageRect = dom.foundryMapStage?.getBoundingClientRect();
    const normalizedPlacement = {
      left: roundPercent(clamp(Number(placement.left) || 0, 0, 100)),
      top: roundPercent(clamp(Number(placement.top) || 0, 0, 100)),
    };

    if (!stageRect?.width || !stageRect?.height) {
      return normalizedPlacement;
    }

    const nearestRoadPlacement = findNearestFoundryRoadPlacement(normalizedPlacement);

    if (!nearestRoadPlacement) {
      return normalizedPlacement;
    }

    if (nearestRoadPlacement.distance <= FOUNDRY_BUILDER_BLUE_NODE_ROPE_RADIUS_PX) {
      return normalizedPlacement;
    }

    const deltaX = nearestRoadPlacement.targetPoint.x - nearestRoadPlacement.projectionPoint.x;
    const deltaY = nearestRoadPlacement.targetPoint.y - nearestRoadPlacement.projectionPoint.y;
    const distance = Math.hypot(deltaX, deltaY);

    if (!distance) {
      return {
        left: roundPercent((nearestRoadPlacement.projectionPoint.x / stageRect.width) * 100),
        top: roundPercent((nearestRoadPlacement.projectionPoint.y / stageRect.height) * 100),
      };
    }

    const ropeScale = FOUNDRY_BUILDER_BLUE_NODE_ROPE_RADIUS_PX / distance;
    const constrainedPoint = {
      x: nearestRoadPlacement.projectionPoint.x + deltaX * ropeScale,
      y: nearestRoadPlacement.projectionPoint.y + deltaY * ropeScale,
    };

    return {
      left: roundPercent(clamp((constrainedPoint.x / stageRect.width) * 100, 0, 100)),
      top: roundPercent(clamp((constrainedPoint.y / stageRect.height) * 100, 0, 100)),
    };
  }

  function syncFoundryBuilderNodeElement(record) {
    const node = dom.foundryMapStage?.querySelector(`[data-builder-node="true"][data-label="${String(record?.label || "")}"]`);

    if (!node || !record) {
      return;
    }

    node.style.left = `${record.left}%`;
    node.style.top = `${record.top}%`;
  }

  function snapFoundryBuilderBlueNodeRecordToRoad(nodeRecord) {
    return false;
  }

  function snapAllFoundryBuilderBlueNodesToRoads() {
    return false;
  }

  function getFoundryBuilderPairedBlueNodeForAsset(assetRecord) {
    if (!assetRecord?.pairedNodeLabel) {
      return null;
    }

    const pairedNode = state.foundryCustomNodes.find(
      (nodeRecord) => nodeRecord.variant === "blue" && nodeRecord.label === String(assetRecord.pairedNodeLabel),
    );

    if (!pairedNode) {
      return null;
    }

    const distance = Math.hypot(
      (Number(assetRecord.left) || 0) - (Number(pairedNode.left) || 0),
      (Number(assetRecord.top) || 0) - (Number(pairedNode.top) || 0),
    );

    return { nodeRecord: pairedNode, distance };
  }

  function getClosestFoundryBuilderBlueNodeForAsset(assetRecord) {
    if (!assetRecord) {
      return null;
    }

    const explicitMatch = getFoundryBuilderPairedBlueNodeForAsset(assetRecord);

    if (explicitMatch) {
      return explicitMatch;
    }

    let bestMatch = null;

    state.foundryCustomNodes.forEach((nodeRecord) => {
      if (nodeRecord.variant !== "blue") {
        return;
      }

      const distance = Math.hypot(
        (Number(assetRecord.left) || 0) - (Number(nodeRecord.left) || 0),
        (Number(assetRecord.top) || 0) - (Number(nodeRecord.top) || 0),
      );

      if (!bestMatch || distance < bestMatch.distance) {
        bestMatch = { nodeRecord, distance };
      }
    });

    if (!bestMatch || bestMatch.distance > FOUNDRY_BUILDER_DEMAND_PAIR_MAX_DISTANCE) {
      return null;
    }

    return bestMatch;
  }

  function pairFoundryBuilderAssetWithBlueNode(assetId, nodeLabel) {
    const nodeRecord = getFoundryBuilderNodeRecord(nodeLabel);

    if (!nodeRecord || nodeRecord.variant !== "blue") {
      return;
    }

    void assetId;
    promptForFoundryBuilderNodeDemand(nodeRecord.label);
    clearFoundryBuilderAssetSelection();
  }

  function getClosestFoundryBuilderAssetForNode(nodeRecord) {
    return null;
  }

  function renderFoundryBuilderDemandState() {
    state.foundryCustomNodes.forEach((nodeRecord) => {
      if (nodeRecord.variant !== "blue") {
        return;
      }

      const nodeElement = getFoundryNodeByLabel(nodeRecord.label);
      const currentDemand = Math.max(Math.round(Number(nodeRecord.demand) || 0), 0);

      if (!nodeElement) {
        return;
      }

      nodeElement.dataset.builderNodeDemand = String(currentDemand);
    });
  }

  function setFoundryBuilderNodeDemand(nodeLabel, demand) {
    const nodeRecord = getFoundryBuilderNodeRecord(nodeLabel);

    if (!nodeRecord || nodeRecord.variant !== "blue") {
      return false;
    }

    nodeRecord.demand = Math.max(Math.round(Number(demand) || 0), 0);
    updateSelectedBusStat();
    renderFoundryBuilderDemandState();
    saveFoundryEditLayoutSnapshot();
    return true;
  }

  function promptForFoundryBuilderNodeDemand(nodeLabel) {
    const nodeRecord = getFoundryBuilderNodeRecord(nodeLabel);

    if (!nodeRecord || nodeRecord.variant !== "blue") {
      return;
    }

    const promptResult = window.prompt(
      "Enter the worker demand for this blue node:",
      String(Math.max(Math.round(Number(nodeRecord.demand) || 0), 0)),
    );

    if (promptResult == null) {
      return;
    }

    const normalizedPrompt = String(promptResult).trim();

    if (!/^\d+$/.test(normalizedPrompt)) {
      showToast("Demand must be a whole number 0 or greater.", {
        title: "Invalid Demand",
      });
      return;
    }

    setFoundryBuilderNodeDemand(nodeRecord.label, Number(normalizedPrompt));
  }

  function getFoundryPickupNodeElement(label) {
    return dom.foundryPickupNodes.find((node) => node.dataset.routeNodeLabel === String(label)) || null;
  }

  function isFactoryFoundryRouteLabel(label) {
    return String(label) === FOUNDRY_FACTORY_START_LABEL;
  }

  function getFoundryRouteNodeElement(label) {
    if (isFactoryFoundryRouteLabel(label)) {
      return dom.foundryFactoryStartNode || null;
    }

    if (isFoundryPickupRouteLabel(label)) {
      return getFoundryPickupNodeElement(label);
    }

    return getFoundryNodeByLabel(label);
  }

  function getFoundryRouteNodeLabel(element) {
    return element?.dataset.routeNodeLabel || element?.dataset.label || "";
  }

  function isSelectableFoundryRouteLabel(label) {
    if (isFactoryFoundryRouteLabel(label)) {
      return Boolean(dom.foundryFactoryStartNode && !dom.foundryFactoryRoundabout?.hidden);
    }

    if (isFoundryPickupRouteLabel(label)) {
      const pickupNode = getFoundryPickupNodeElement(label);
      return Boolean(pickupNode && !pickupNode.hidden);
    }

    const node = getFoundryNodeByLabel(label);
    return Boolean(node && !isPerimeterFoundryNode(node));
  }

  function isFoundryBrownRouteLabel(label) {
    return isSelectableFoundryRouteLabel(label) && !isFactoryFoundryRouteLabel(label) && !isFoundryDemandRouteLabel(label);
  }

  function isFoundryIntersectionRouteLabel(label) {
    return !isFactoryFoundryRouteLabel(label) && !isFoundryPickupRouteLabel(label) && isSelectableFoundryRouteLabel(label);
  }

  function getMostRecentFoundryBrownLabel(routeNodeLabels) {
    if (!Array.isArray(routeNodeLabels) || routeNodeLabels.length < 2) {
      return "";
    }

    for (let index = routeNodeLabels.length - 2; index >= 0; index -= 1) {
      const label = routeNodeLabels[index];

      if (isFoundryBrownRouteLabel(label)) {
        return String(label);
      }
    }

    return "";
  }

  function getMostRecentFoundryIntersectionLabel(routeNodeLabels) {
    if (!Array.isArray(routeNodeLabels) || routeNodeLabels.length < 2) {
      return "";
    }

    for (let index = routeNodeLabels.length - 2; index >= 0; index -= 1) {
      const label = routeNodeLabels[index];

      if (isFoundryIntersectionRouteLabel(label)) {
        return label;
      }
    }

    return "";
  }

  function getFoundryRouteAdjacency() {
    const adjacency = new Map();

    state.foundryRoads.forEach((road) => {
      const from = String(road.from);
      const to = String(road.to);

      if (!adjacency.has(from)) {
        adjacency.set(from, new Set());
      }

      if (!adjacency.has(to)) {
        adjacency.set(to, new Set());
      }

      adjacency.get(from).add(to);
      adjacency.get(to).add(from);
    });

    const factoryConnections = FOUNDRY_FACTORY_START_CONNECTIONS.filter((label) => isSelectableFoundryRouteLabel(label));

    if (factoryConnections.length) {
      adjacency.set(FOUNDRY_FACTORY_START_LABEL, new Set(factoryConnections));

      factoryConnections.forEach((label) => {
        if (!adjacency.has(label)) {
          adjacency.set(label, new Set());
        }

        adjacency.get(label).add(FOUNDRY_FACTORY_START_LABEL);
      });
    }

    Object.entries(state.foundryPickupLayouts || {}).forEach(([pickupLabel, placement]) => {
      const from = String(placement?.from || "");
      const to = String(placement?.to || "");

      if (!from || !to || !isSelectableFoundryRouteLabel(pickupLabel)) {
        return;
      }

      if (!adjacency.has(pickupLabel)) {
        adjacency.set(pickupLabel, new Set());
      }

      [from, to].forEach((neighborLabel) => {
        if (!adjacency.has(neighborLabel)) {
          adjacency.set(neighborLabel, new Set());
        }

        adjacency.get(pickupLabel).add(neighborLabel);
        adjacency.get(neighborLabel).add(pickupLabel);
      });
    });

    return adjacency;
  }

  function getFoundryExtendedRouteCandidates(currentLabel, adjacency = getFoundryRouteAdjacency()) {
    const startLabel = String(currentLabel || "");

    if (!startLabel) {
      return [];
    }

    const candidateLabels = new Set();
    const visitedDemandLabels = new Set();
    const directNeighbors = Array.from(adjacency.get(startLabel) || []);
    const queue = [];

    directNeighbors.forEach((label) => {
      const nextLabel = String(label || "");

      if (!nextLabel || nextLabel === startLabel) {
        return;
      }

      candidateLabels.add(nextLabel);

      if (isFoundryDemandRouteLabel(nextLabel) && !visitedDemandLabels.has(nextLabel)) {
        visitedDemandLabels.add(nextLabel);
        queue.push(nextLabel);
      }
    });

    while (queue.length) {
      const demandLabel = queue.shift();

      Array.from(adjacency.get(demandLabel) || []).forEach((neighborLabel) => {
        const nextLabel = String(neighborLabel || "");

        if (!nextLabel || nextLabel === startLabel) {
          return;
        }

        candidateLabels.add(nextLabel);

        if (isFoundryDemandRouteLabel(nextLabel) && !visitedDemandLabels.has(nextLabel)) {
          visitedDemandLabels.add(nextLabel);
          queue.push(nextLabel);
        }
      });
    }

    return Array.from(candidateLabels);
  }

  function findFoundrySkippedBlueTransferPath(startLabel, endLabel, adjacency = getFoundryRouteAdjacency()) {
    const fromLabel = String(startLabel || "");
    const toLabel = String(endLabel || "");

    if (!fromLabel || !toLabel || fromLabel === toLabel) {
      return [];
    }

    const directNeighbors = adjacency.get(fromLabel);

    if (directNeighbors?.has(toLabel)) {
      return [];
    }

    const visitedDemandLabels = new Set();
    const queue = [];

    Array.from(directNeighbors || []).forEach((label) => {
      const nextLabel = String(label || "");

      if (!nextLabel || nextLabel === fromLabel || !isFoundryDemandRouteLabel(nextLabel) || visitedDemandLabels.has(nextLabel)) {
        return;
      }

      visitedDemandLabels.add(nextLabel);
      queue.push({
        label: nextLabel,
        path: [fromLabel, nextLabel],
      });
    });

    while (queue.length) {
      const currentStep = queue.shift();

      if (!currentStep) {
        continue;
      }

      for (const neighborLabel of Array.from(adjacency.get(currentStep.label) || [])) {
        const nextLabel = String(neighborLabel || "");

        if (!nextLabel || nextLabel === fromLabel) {
          continue;
        }

        if (nextLabel === toLabel) {
          return [...currentStep.path, toLabel];
        }

        if (isFoundryDemandRouteLabel(nextLabel) && !visitedDemandLabels.has(nextLabel)) {
          visitedDemandLabels.add(nextLabel);
          queue.push({
            label: nextLabel,
            path: [...currentStep.path, nextLabel],
          });
        }
      }
    }

    return [];
  }

  function getReachableFoundryRouteLabels(bus = getSelectedFleetBus()) {
    if (!bus) {
      return [];
    }

    const routeNodeLabels = getFleetBusRouteNodeLabels(bus);

    if (!routeNodeLabels.length) {
      return isSelectableFoundryRouteLabel(FOUNDRY_FACTORY_START_LABEL) ? [FOUNDRY_FACTORY_START_LABEL] : [];
    }

    const currentLabel = routeNodeLabels[routeNodeLabels.length - 1];
    const previousLabel = routeNodeLabels.length > 1 ? routeNodeLabels[routeNodeLabels.length - 2] : "";
    const forbiddenBrownLabel = getMostRecentFoundryBrownLabel(routeNodeLabels);
    const adjacency = getFoundryRouteAdjacency();
    const candidateLabels = getFoundryExtendedRouteCandidates(currentLabel, adjacency);

    return candidateLabels.filter(
      (label) => (
        label !== previousLabel &&
        label !== forbiddenBrownLabel &&
        isSelectableFoundryRouteLabel(label) &&
        canFleetBusAppendRouteNode(bus, label)
      ),
    );
  }

  function isPerimeterFoundryNode(node) {
    const explicitRole = String(node?.dataset?.nodeRole || "");

    if (explicitRole === "perimeter") {
      return true;
    }

    if (explicitRole === "interior") {
      return false;
    }

    const label = Number(node?.dataset.label);
    return Number.isFinite(label) && label >= 17;
  }

  function getFoundryRoadKey(firstLabel, secondLabel) {
    return [String(firstLabel), String(secondLabel)]
      .sort((first, second) => Number(first) - Number(second))
      .join(":");
  }

  function getFoundryRoadNeighbors(label, roads = state.foundryRoads) {
    const targetLabel = String(label || "");

    if (!targetLabel) {
      return [];
    }

    return Array.from(new Set(
      (Array.isArray(roads) ? roads : []).reduce((neighbors, road) => {
        if (String(road?.from || "") === targetLabel) {
          neighbors.push(String(road.to));
        } else if (String(road?.to || "") === targetLabel) {
          neighbors.push(String(road.from));
        }

        return neighbors;
      }, []),
    ));
  }

  function isFoundryBuilderBlueNodeLabel(label) {
    return getFoundryBuilderNodeRecord(label)?.variant === "blue";
  }

  function isFoundryBrownBuilderNodeLabel(label) {
    return Boolean(getFoundryNodeByLabel(label)) && !isFoundryBuilderBlueNodeLabel(label);
  }

  function findFoundryInlineBlueSplitNodeLabel(firstLabel, secondLabel, roads = state.foundryRoads) {
    const startLabel = String(firstLabel || "");
    const endLabel = String(secondLabel || "");

    if (!startLabel || !endLabel) {
      return "";
    }

    const matchingBlueNode = state.foundryCustomNodes.find((nodeRecord) => {
      if (nodeRecord.variant !== "blue") {
        return false;
      }

      const neighborLabels = getFoundryRoadNeighbors(nodeRecord.label, roads);
      return neighborLabels.length === 2 && neighborLabels.includes(startLabel) && neighborLabels.includes(endLabel);
    });

    return matchingBlueNode ? String(matchingBlueNode.label) : "";
  }

  function collapseFoundryInlineBlueDirectRoad(blueLabel) {
    if (!isFoundryBuilderBlueNodeLabel(blueLabel)) {
      return false;
    }

    const neighborLabels = getFoundryRoadNeighbors(blueLabel);
    const brownNeighbors = neighborLabels.filter((label) => isFoundryBrownBuilderNodeLabel(label));

    if (neighborLabels.length !== 2 || brownNeighbors.length !== 2) {
      return false;
    }

    const directRoadKey = getFoundryRoadKey(brownNeighbors[0], brownNeighbors[1]);
    const hasDirectRoad = state.foundryRoads.some((road) => getFoundryRoadKey(road.from, road.to) === directRoadKey);

    if (!hasDirectRoad) {
      return false;
    }

    state.foundryRoads = state.foundryRoads.filter(
      (road) => getFoundryRoadKey(road.from, road.to) !== directRoadKey,
    );
    return true;
  }

  function validateFoundryEditorRoadAddition(firstLabel, secondLabel) {
    const fromLabel = String(firstLabel || "");
    const toLabel = String(secondLabel || "");
    const roadKey = getFoundryRoadKey(fromLabel, toLabel);

    if (!fromLabel || !toLabel || fromLabel === toLabel) {
      return {
        isAllowed: false,
        title: "Invalid Road",
        message: "Choose two different nodes to create a road.",
      };
    }

    if (state.foundryRoads.some((road) => getFoundryRoadKey(road.from, road.to) === roadKey)) {
      return {
        isAllowed: false,
        title: "Duplicate Road",
        message: "Those two nodes are already connected.",
      };
    }

    if (
      isFoundryBrownBuilderNodeLabel(fromLabel)
      && isFoundryBrownBuilderNodeLabel(toLabel)
      && findFoundryInlineBlueSplitNodeLabel(fromLabel, toLabel)
    ) {
      return {
        isAllowed: false,
        title: "Inline Stop Locked",
        message: "That corridor is already owned by a blue inline stop. Delete one split segment first if you want a direct road again.",
      };
    }

    const blockedBlueLabel = [fromLabel, toLabel].find((label) => {
      return isFoundryBuilderBlueNodeLabel(label) && getFoundryRoadNeighbors(label).length >= 2;
    });

    if (blockedBlueLabel) {
      return {
        isAllowed: false,
        title: "Blue Node Full",
        message: "Blue demand nodes can connect to at most two roads.",
      };
    }

    return { isAllowed: true };
  }

  function measureFoundryNodeCenter(node) {
    const stage = dom.foundryMapStage;

    if (!stage || !node) {
      return { x: 0, y: 0 };
    }

    const stageRect = stage.getBoundingClientRect();
    const nodeRect = node.getBoundingClientRect();

    return {
      x: nodeRect.left - stageRect.left + nodeRect.width / 2,
      y: nodeRect.top - stageRect.top + nodeRect.height / 2,
    };
  }

  function measureFoundryRouteNodeCenter(label) {
    return measureFoundryNodeCenter(getFoundryRouteNodeElement(label));
  }

  function syncFoundryMapLayout(settlePasses = 0) {
    renderFoundryRoads();

    if (settlePasses > 0) {
      window.requestAnimationFrame(() => {
        syncFoundryMapLayout(settlePasses - 1);
      });
    }
  }

  function projectFoundryPointToBoundary(targetPoint, referencePoint, stageRect) {
    const deltaX = targetPoint.x - referencePoint.x;
    const deltaY = targetPoint.y - referencePoint.y;
    const candidates = [];

    if (deltaX !== 0) {
      const leftT = (0 - referencePoint.x) / deltaX;
      const leftY = referencePoint.y + leftT * deltaY;

      if (leftT >= 0 && leftT <= 1 && leftY >= 0 && leftY <= stageRect.height) {
        candidates.push({ x: 0, y: leftY, t: leftT });
      }

      const rightT = (stageRect.width - referencePoint.x) / deltaX;
      const rightY = referencePoint.y + rightT * deltaY;

      if (rightT >= 0 && rightT <= 1 && rightY >= 0 && rightY <= stageRect.height) {
        candidates.push({ x: stageRect.width, y: rightY, t: rightT });
      }
    }

    if (deltaY !== 0) {
      const topT = (0 - referencePoint.y) / deltaY;
      const topX = referencePoint.x + topT * deltaX;

      if (topT >= 0 && topT <= 1 && topX >= 0 && topX <= stageRect.width) {
        candidates.push({ x: topX, y: 0, t: topT });
      }

      const bottomT = (stageRect.height - referencePoint.y) / deltaY;
      const bottomX = referencePoint.x + bottomT * deltaX;

      if (bottomT >= 0 && bottomT <= 1 && bottomX >= 0 && bottomX <= stageRect.width) {
        candidates.push({ x: bottomX, y: stageRect.height, t: bottomT });
      }
    }

    if (!candidates.length) {
      return targetPoint;
    }

    candidates.sort((first, second) => second.t - first.t);

    return {
      x: candidates[0].x,
      y: candidates[0].y,
    };
  }

  function resolveFoundryRoadEndpoint(node, otherPoint, stageRect) {
    const center = measureFoundryNodeCenter(node);

    if (!isPerimeterFoundryNode(node)) {
      return center;
    }

    const boundaryPoint = projectFoundryPointToBoundary(center, otherPoint, stageRect);
    const deltaX = boundaryPoint.x - otherPoint.x;
    const deltaY = boundaryPoint.y - otherPoint.y;
    const distance = Math.hypot(deltaX, deltaY);

    if (!distance) {
      return boundaryPoint;
    }

    const overshoot = 24;

    return {
      x: boundaryPoint.x + (deltaX / distance) * overshoot,
      y: boundaryPoint.y + (deltaY / distance) * overshoot,
    };
  }

  function resolveFoundryBoundaryIntersection(node, otherPoint, stageRect) {
    const center = measureFoundryNodeCenter(node);

    if (!isPerimeterFoundryNode(node)) {
      return center;
    }

    return projectFoundryPointToBoundary(center, otherPoint, stageRect);
  }

  function getFoundryPerimeterSide(node) {
    const position = measureFoundryNodePercent(node);

    if (position.top <= 1.5) {
      return "top";
    }

    if (position.left >= 98.5) {
      return "right";
    }

    if (position.top >= 98.5) {
      return "bottom";
    }

    if (position.left <= 1.5) {
      return "left";
    }

    return "";
  }

  function findShortestFoundryPath(startLabel, endLabel, adjacency) {
    const queue = [[String(startLabel)]];
    const visited = new Set([String(startLabel)]);

    while (queue.length) {
      const path = queue.shift();
      const current = path[path.length - 1];

      if (current === String(endLabel)) {
        return path;
      }

      (adjacency.get(current) || []).forEach((nextLabel) => {
        if (visited.has(nextLabel)) {
          return;
        }

        visited.add(nextLabel);
        queue.push([...path, nextLabel]);
      });
    }

    return null;
  }

  function calculateFoundryPolygonArea(points) {
    if (!points?.length) {
      return Number.POSITIVE_INFINITY;
    }

    let area = 0;

    points.forEach((point, index) => {
      const nextPoint = points[(index + 1) % points.length];
      area += point.x * nextPoint.y - nextPoint.x * point.y;
    });

    return Math.abs(area / 2);
  }

  function buildFoundryNorthwestZonePoints(pathLabels, stageRect) {
    if (!Array.isArray(pathLabels) || pathLabels.length < 2) {
      return null;
    }

    const firstNode = getFoundryNodeByLabel(pathLabels[0]);
    const secondNode = getFoundryNodeByLabel(pathLabels[1]);
    const lastNode = getFoundryNodeByLabel(pathLabels[pathLabels.length - 1]);
    const previousNode = getFoundryNodeByLabel(pathLabels[pathLabels.length - 2]);

    if (!firstNode || !secondNode || !lastNode || !previousNode) {
      return null;
    }

    const startPoint = resolveFoundryBoundaryIntersection(firstNode, measureFoundryNodeCenter(secondNode), stageRect);
    const endPoint = resolveFoundryBoundaryIntersection(lastNode, measureFoundryNodeCenter(previousNode), stageRect);

    if (startPoint.y > 1 || endPoint.x > 1) {
      return null;
    }

    const interiorPoints = pathLabels.slice(1, -1).map((label) => measureFoundryNodeCenter(getFoundryNodeByLabel(label)));

    return [
      { x: 0, y: 0 },
      startPoint,
      ...interiorPoints,
      endPoint,
    ];
  }

  function renderFoundryNorthwestZone(stageRect) {
    const zoneLayer = dom.foundryZoneLayer;

    if (!zoneLayer) {
      return;
    }

    zoneLayer.innerHTML = "";

    if (!state.foundryRoads.length) {
      return;
    }

    const adjacency = new Map();

    state.foundryRoads.forEach((road) => {
      const from = String(road.from);
      const to = String(road.to);

      if (!adjacency.has(from)) {
        adjacency.set(from, []);
      }

      if (!adjacency.has(to)) {
        adjacency.set(to, []);
      }

      adjacency.get(from).push(to);
      adjacency.get(to).push(from);
    });

    const topBoundaryLabels = [];
    const leftBoundaryLabels = [];

    dom.foundryMapNodes.forEach((node) => {
      const label = node.dataset.label || "";

      if (!adjacency.has(label) || !isPerimeterFoundryNode(node)) {
        return;
      }

      const side = getFoundryPerimeterSide(node);

      if (side === "top") {
        topBoundaryLabels.push(label);
      } else if (side === "left") {
        leftBoundaryLabels.push(label);
      }
    });

    let bestZone = null;

    topBoundaryLabels.forEach((topLabel) => {
      leftBoundaryLabels.forEach((leftLabel) => {
        const path = findShortestFoundryPath(topLabel, leftLabel, adjacency);

        if (!path || path.length < 2) {
          return;
        }

        const hasInteriorPerimeterNode = path.slice(1, -1).some((label) => isPerimeterFoundryNode(getFoundryNodeByLabel(label)));

        if (hasInteriorPerimeterNode) {
          return;
        }

        const points = buildFoundryNorthwestZonePoints(path, stageRect);

        if (!points) {
          return;
        }

        const area = calculateFoundryPolygonArea(points);

        if (!Number.isFinite(area) || area < 4000) {
          return;
        }

        if (!bestZone || area < bestZone.area) {
          bestZone = { points, area };
        }
      });
    });

    if (!bestZone) {
      return;
    }

    const zoneElement = document.createElement("div");
    const polygon = bestZone.points
      .map((point) => `${(point.x / stageRect.width) * 100}% ${(point.y / stageRect.height) * 100}%`)
      .join(", ");

    zoneElement.className = "map-zone map-zone--northwest";
    zoneElement.style.clipPath = `polygon(${polygon})`;
    zoneLayer.append(zoneElement);
  }

  function renderFoundryFactoryRoundabout() {
    const roundabout = dom.foundryFactoryRoundabout;
    const geometry = getFoundryFactoryRoundaboutGeometry();

    if (!roundabout || !geometry) {
      if (roundabout) {
        roundabout.hidden = true;
      }
      return;
    }

    roundabout.hidden = false;
    roundabout.style.left = `${geometry.anchorX}px`;
    roundabout.style.top = `${geometry.anchorY}px`;
    roundabout.style.transform = `translate(-50%, -100%) rotate(${geometry.angleDegrees}deg)`;
  }

  function getFoundryFactoryRoundaboutGeometry() {
    const lowerNode = getFoundryNodeByLabel("1");
    const upperNode = getFoundryNodeByLabel("2");

    if (!lowerNode || !upperNode) {
      return null;
    }

    const hasAnchorRoad = state.foundryRoads.some(
      (road) => getFoundryRoadKey(road.from, road.to) === getFoundryRoadKey("1", "2"),
    );

    if (!hasAnchorRoad) {
      return null;
    }

    const start = measureFoundryNodeCenter(lowerNode);
    const end = measureFoundryNodeCenter(upperNode);
    const deltaX = end.x - start.x;
    const deltaY = end.y - start.y;
    const angleRadians = Math.atan2(deltaY, deltaX);
    const angleDegrees = angleRadians * (180 / Math.PI);
    const anchorX = start.x + deltaX * FOUNDRY_FACTORY_ROUNDABOUT_LAYOUT.anchorRatio;
    const anchorY = start.y + deltaY * FOUNDRY_FACTORY_ROUNDABOUT_LAYOUT.anchorRatio;
    const { width, height } = FOUNDRY_FACTORY_ROUNDABOUT_LAYOUT;
    const localOriginX = width / 2;
    const localOriginY = height;

    function projectLocalPoint(localX, localY) {
      const translatedX = localX - localOriginX;
      const translatedY = localY - localOriginY;

      return {
        x: anchorX + translatedX * Math.cos(angleRadians) - translatedY * Math.sin(angleRadians),
        y: anchorY + translatedX * Math.sin(angleRadians) + translatedY * Math.cos(angleRadians),
      };
    }

    return {
      anchorX,
      anchorY,
      angleRadians,
      angleDegrees,
      projectLocalPoint,
      topPoint: projectLocalPoint(width / 2, 0),
      leftPoint: projectLocalPoint(0, height),
      rightPoint: projectLocalPoint(width, height),
    };
  }

  function getFoundryFactoryRoundaboutRoutePoints(otherLabel) {
    const geometry = getFoundryFactoryRoundaboutGeometry();

    if (!geometry) {
      return [];
    }

    const endAngle = String(otherLabel) === "1" ? -Math.PI : 0;
    const points = [];
    const { width, height, curveSamples } = FOUNDRY_FACTORY_ROUNDABOUT_LAYOUT;
    const centerX = width / 2;
    const centerY = height;
    const radiusX = width / 2;
    const radiusY = height;

    for (let index = 0; index <= curveSamples; index += 1) {
      const progress = index / curveSamples;
      const theta = (-Math.PI / 2) + ((endAngle + (Math.PI / 2)) * progress);
      points.push(
        geometry.projectLocalPoint(
          centerX + radiusX * Math.cos(theta),
          centerY + radiusY * Math.sin(theta),
        ),
      );
    }

    return points;
  }

  function appendFoundryRouteSegment(roadLayer, start, end, isActive) {
    const deltaX = end.x - start.x;
    const deltaY = end.y - start.y;
    const distance = Math.hypot(deltaX, deltaY);

    if (!distance) {
      return;
    }

    const angle = Math.atan2(deltaY, deltaX) * (180 / Math.PI);
    const routeElement = document.createElement("span");

    routeElement.className = `map-road-link map-road-link--route${isActive ? " map-road-link--route-active" : ""}`;
    routeElement.style.left = `${start.x}px`;
    routeElement.style.top = `${start.y}px`;
    routeElement.style.width = `${distance}px`;
    routeElement.style.transform = `translateY(-50%) rotate(${angle}deg)`;
    roadLayer.append(routeElement);
  }

  function appendFoundryRoutePolyline(roadLayer, points, isActive) {
    for (let index = 1; index < points.length; index += 1) {
      appendFoundryRouteSegment(roadLayer, points[index - 1], points[index], isActive);
    }
  }

  function getFoundryRouteSegmentPoints(previousLabel, currentLabel) {
    const includesFactory = isFactoryFoundryRouteLabel(previousLabel) || isFactoryFoundryRouteLabel(currentLabel);

    if (includesFactory) {
      const otherLabel = isFactoryFoundryRouteLabel(previousLabel) ? currentLabel : previousLabel;

      if (FOUNDRY_FACTORY_START_CONNECTIONS.includes(String(otherLabel))) {
        const otherPoint = measureFoundryRouteNodeCenter(otherLabel);
        const roundaboutPoints = getFoundryFactoryRoundaboutRoutePoints(otherLabel);

        if (roundaboutPoints.length) {
          return isFactoryFoundryRouteLabel(previousLabel)
            ? [...roundaboutPoints, otherPoint]
            : [otherPoint, ...roundaboutPoints.slice().reverse()];
        }
      }
    }

    const skippedBluePath = findFoundrySkippedBlueTransferPath(previousLabel, currentLabel);

    if (skippedBluePath.length > 2) {
      return skippedBluePath.map((label) => measureFoundryRouteNodeCenter(label));
    }

    return [
      measureFoundryRouteNodeCenter(previousLabel),
      measureFoundryRouteNodeCenter(currentLabel),
    ];
  }

  function buildFoundryRoutePolylinePoints(routeNodeLabels) {
    const routePoints = [];

    for (let index = 1; index < routeNodeLabels.length; index += 1) {
      const segmentPoints = getFoundryRouteSegmentPoints(routeNodeLabels[index - 1], routeNodeLabels[index]);

      if (!segmentPoints.length) {
        continue;
      }

      routePoints.push(...(routePoints.length ? segmentPoints.slice(1) : segmentPoints));
    }

    return routePoints;
  }

  function buildFoundryRouteAnimationSegments(points) {
    const segments = [];
    let totalLength = 0;

    for (let index = 1; index < points.length; index += 1) {
      const start = points[index - 1];
      const end = points[index];
      const deltaX = end.x - start.x;
      const deltaY = end.y - start.y;
      const length = Math.hypot(deltaX, deltaY);

      if (!length) {
        continue;
      }

      segments.push({
        start,
        end,
        length,
        angleDegrees: Math.atan2(deltaY, deltaX) * (180 / Math.PI),
      });
      totalLength += length;
    }

    return { segments, totalLength };
  }

  function getFoundryAnimationPosition(animationSegments, progressLength) {
    if (!animationSegments.segments.length) {
      return null;
    }

    let traversed = 0;

    for (const segment of animationSegments.segments) {
      if (progressLength <= traversed + segment.length) {
        const localProgress = (progressLength - traversed) / segment.length;

        return {
          x: segment.start.x + (segment.end.x - segment.start.x) * localProgress,
          y: segment.start.y + (segment.end.y - segment.start.y) * localProgress,
          angleDegrees: segment.angleDegrees,
        };
      }

      traversed += segment.length;
    }

    const lastSegment = animationSegments.segments[animationSegments.segments.length - 1];

    return {
      x: lastSegment.end.x,
      y: lastSegment.end.y,
      angleDegrees: lastSegment.angleDegrees,
    };
  }

  function getFoundryRoutePauseMs(label, bus, routeIndex, routeNodeLabels) {
    if (isFactoryFoundryRouteLabel(label)) {
      return routeIndex === routeNodeLabels.length - 1 ? FOUNDRY_ROUTE_DEPOT_PAUSE_MS : 0;
    }

    if (isFoundryDemandRouteLabel(label)) {
      const boardedWorkers = Math.max(Number(bus?.draftPickupPlan?.[routeIndex]) || 0, 0);
      return boardedWorkers > 0 ? FOUNDRY_ROUTE_PICKUP_PAUSE_MS : FOUNDRY_ROUTE_INTERSECTION_PAUSE_MS;
    }

    return FOUNDRY_ROUTE_INTERSECTION_PAUSE_MS;
  }

  function buildFoundryRouteAnimationTimeline(bus) {
    const routeNodeLabels = getFleetBusRouteNodeLabels(bus);
    const phases = [];
    let totalDurationMs = 0;
    let lastPosition = null;

    for (let index = 1; index < routeNodeLabels.length; index += 1) {
      const segmentPoints = getFoundryRouteSegmentPoints(routeNodeLabels[index - 1], routeNodeLabels[index]);
      const animationSegments = buildFoundryRouteAnimationSegments(segmentPoints);

      if (animationSegments.totalLength) {
        const travelDurationMs = Math.max(
          (animationSegments.totalLength / FOUNDRY_ROUTE_ANIMATION_PIXELS_PER_SECOND) * 1000,
          1,
        );

        phases.push({
          type: "travel",
          durationMs: travelDurationMs,
          totalLength: animationSegments.totalLength,
          segments: animationSegments.segments,
        });
        totalDurationMs += travelDurationMs;
        lastPosition = getFoundryAnimationPosition(animationSegments, animationSegments.totalLength);
      } else {
        lastPosition = measureFoundryRouteNodeCenter(routeNodeLabels[index]);
      }

      const pauseMs = getFoundryRoutePauseMs(routeNodeLabels[index], bus, index, routeNodeLabels);

      if (pauseMs > 0 && lastPosition) {
        phases.push({
          type: "pause",
          durationMs: pauseMs,
          position: lastPosition,
        });
        totalDurationMs += pauseMs;
      }
    }

    return { phases, totalDurationMs, lastPosition };
  }

  function getFoundryReplayTravelDurationMs(previousLabel, currentLabel) {
    const previousIsPickup = isFoundryPickupRouteLabel(previousLabel);
    const currentIsPickup = isFoundryPickupRouteLabel(currentLabel);

    if (previousIsPickup !== currentIsPickup) {
      return 1500;
    }

    return Math.max(getFoundryRouteSelectionCost(currentLabel) * FOUNDRY_REPLAY_MS_PER_MINUTE, 1);
  }

  function getFoundryReplayPauseDurationMs(routeRecord, routeIndex, label) {
    if (!isFoundryDemandRouteLabel(label)) {
      return 0;
    }

    const boardedWorkers = Math.max(Number(routeRecord?.pickupPlan?.[routeIndex]) || 0, 0);
    return boardedWorkers > 0 ? 1000 : 0;
  }

  function buildFoundryReplayRouteTimeline(routeRecord) {
    const safeRouteNodeLabels = Array.isArray(routeRecord?.routeNodeLabels) ? routeRecord.routeNodeLabels : [];
    const phases = [];
    let totalDurationMs = 0;
    let lastPosition = null;

    for (let index = 1; index < safeRouteNodeLabels.length; index += 1) {
      const previousLabel = safeRouteNodeLabels[index - 1];
      const currentLabel = safeRouteNodeLabels[index];
      const segmentPoints = getFoundryRouteSegmentPoints(previousLabel, currentLabel);
      const animationSegments = buildFoundryRouteAnimationSegments(segmentPoints);
      const travelDurationMs = getFoundryReplayTravelDurationMs(previousLabel, currentLabel);
      const boardedWorkers = Math.max(Number(routeRecord?.pickupPlan?.[index]) || 0, 0);
      const demandLabel = boardedWorkers > 0 && isFoundryDemandRouteLabel(currentLabel) ? String(currentLabel) : "";

      if (animationSegments.totalLength) {
        phases.push({
          type: "travel",
          durationMs: travelDurationMs,
          totalLength: animationSegments.totalLength,
          segments: animationSegments.segments,
        });
        totalDurationMs += travelDurationMs;
        lastPosition = getFoundryAnimationPosition(animationSegments, animationSegments.totalLength);
      } else {
        const stationaryPosition = measureFoundryRouteNodeCenter(currentLabel);

        if (!stationaryPosition) {
          continue;
        }

        phases.push({
          type: "pause",
          durationMs: travelDurationMs,
          position: stationaryPosition,
        });
        totalDurationMs += travelDurationMs;
        lastPosition = stationaryPosition;
      }

      const pickupPauseMs = boardedWorkers > 0 ? 1000 : getFoundryReplayPauseDurationMs(routeRecord, index, currentLabel);

      if (pickupPauseMs > 0 && lastPosition) {
        phases.push({
          type: "pause",
          durationMs: pickupPauseMs,
          position: lastPosition,
          pickupDemandLabel: demandLabel,
          boardedWorkers,
        });
        totalDurationMs += pickupPauseMs;
      }
    }

    if (!lastPosition && safeRouteNodeLabels.length) {
      lastPosition =
        measureFoundryRouteNodeCenter(safeRouteNodeLabels[safeRouteNodeLabels.length - 1]) ||
        measureFoundryRouteNodeCenter(safeRouteNodeLabels[0]);
    }

    return { phases, totalDurationMs, lastPosition };
  }

  function getFoundryAnimationTimelineState(animationTimeline, elapsedMs) {
    if (!animationTimeline?.phases?.length) {
      return {
        position: animationTimeline?.lastPosition || null,
        phase: null,
        phaseIndex: -1,
      };
    }

    let traversedMs = 0;
    let lastPhase = null;
    let lastPhaseIndex = -1;

    for (let index = 0; index < animationTimeline.phases.length; index += 1) {
      const phase = animationTimeline.phases[index];
      const phaseEnd = traversedMs + phase.durationMs;

      if (elapsedMs <= phaseEnd) {
        if (phase.type === "pause") {
          return {
            position: phase.position,
            phase,
            phaseIndex: index,
          };
        }

        const localProgress = phase.durationMs ? clamp((elapsedMs - traversedMs) / phase.durationMs, 0, 1) : 1;
        return {
          position: getFoundryAnimationPosition({ segments: phase.segments }, phase.totalLength * localProgress),
          phase,
          phaseIndex: index,
        };
      }

      traversedMs = phaseEnd;
      lastPhase = phase;
      lastPhaseIndex = index;
    }

    return {
      position: animationTimeline.lastPosition || null,
      phase: lastPhase,
      phaseIndex: lastPhaseIndex,
    };
  }

  function getFoundryAnimationTimelinePosition(animationTimeline, elapsedMs) {
    return getFoundryAnimationTimelineState(animationTimeline, elapsedMs).position;
  }

  function getFoundryAnimationRunner(runnerId) {
    if (!dom.foundryAnimationLayer) {
      return null;
    }

    return Array.from(dom.foundryAnimationLayer.children).find((child) => {
      return child instanceof HTMLElement && child.dataset.routeRunnerId === runnerId;
    }) || null;
  }

  function ensureFoundryAnimationRunner(runnerId, busType) {
    const animationLayer = dom.foundryAnimationLayer;

    if (!animationLayer) {
      return null;
    }

    let runner = getFoundryAnimationRunner(runnerId);

    if (runner) {
      return runner;
    }

    runner = document.createElement("div");
    runner.className = "map-route-runner";
    runner.dataset.routeRunnerId = runnerId;
    runner.innerHTML = `
      <span class="map-bus map-bus--${escapeHtml(busType)}" aria-hidden="true">
        <span class="map-bus__window"></span>
        <span class="map-bus__mark"></span>
      </span>
    `;
    animationLayer.append(runner);

    return runner;
  }

  function removeFoundryAnimationRunner(runnerId) {
    getFoundryAnimationRunner(runnerId)?.remove();
  }

  function clearFoundryAnimationRunners() {
    dom.foundryAnimationLayer?.replaceChildren();
  }

  function renderFoundryAnimatingBus(position, busType, runnerId = FOUNDRY_LIVE_RUNNER_ID) {
    const animationLayer = dom.foundryAnimationLayer;

    if (!animationLayer || !position) {
      return;
    }

    const runner = ensureFoundryAnimationRunner(runnerId, busType);

    if (!runner) {
      return;
    }

    const busElement = runner.querySelector(".map-bus");
    const angleDegrees = Number(position.angleDegrees) || 0;
    const shouldFlipLeft = angleDegrees > 90 || angleDegrees < -90;
    const displayAngleDegrees = shouldFlipLeft
      ? angleDegrees + (angleDegrees > 90 ? -180 : 180)
      : angleDegrees;

    runner.style.left = `${position.x}px`;
    runner.style.top = `${position.y}px`;
    runner.style.transform = `translate(-50%, -50%) rotate(${displayAngleDegrees}deg)`;

    if (busElement) {
      busElement.style.transform = shouldFlipLeft ? "scaleX(-1)" : "scaleX(1)";
    }
  }

  function clearFoundryAnimatingBus() {
    if (state.foundryAnimationFrame) {
      window.cancelAnimationFrame(state.foundryAnimationFrame);
      state.foundryAnimationFrame = 0;
    }

    removeFoundryAnimationRunner(FOUNDRY_LIVE_RUNNER_ID);
    state.foundryAnimatingBusId = "";
  }

  function buildFoundryReplayPlanFromBuses(buses, runnerPrefix) {
    const replayBuses = (Array.isArray(buses) ? buses : []).reduce((replayFleet, bus) => {
      const routes = getFleetBusCompletedRouteHistory(bus).reduce((committedRoutes, route, routeIndex) => {
        const timeline = buildFoundryReplayRouteTimeline(route);

        if (!timeline.totalDurationMs || !timeline.lastPosition) {
          return committedRoutes;
        }

        const startMs = Math.max(Number(route?.startMinute) || 0, 0) * FOUNDRY_REPLAY_MS_PER_MINUTE;

        committedRoutes.push({
          routeIndex,
          startMs,
          endMs: startMs + timeline.totalDurationMs,
          timeline,
        });
        return committedRoutes;
      }, []);

      if (!routes.length) {
        return replayFleet;
      }

      replayFleet.push({
        runnerId: `${runnerPrefix}-${bus.id}`,
        busType: bus.type,
        routes,
      });
      return replayFleet;
    }, []);

    const totalDurationMs = replayBuses.reduce((longestDuration, replayBus) => {
      const replayBusEndMs = replayBus.routes.reduce((latestEndMs, route) => Math.max(latestEndMs, route.endMs), 0);
      return Math.max(longestDuration, replayBusEndMs);
    }, 0);

    return { replayBuses, totalDurationMs };
  }

  function buildFoundrySolutionReplayPlan() {
    return buildFoundryReplayPlanFromBuses(state.purchasedFleet, "solution-replay");
  }

  function buildFoundryOptimalReplayPlan() {
    const snapshot = FOUNDRY_OPTIMAL_SCHEDULE_REPLAY_SNAPSHOTS[state.currentLevel];
    return buildFoundryReplayPlanFromBuses(snapshot?.buses || [], "optimal-replay");
  }

  function getFoundrySolutionReplayRouteAtElapsed(replayBus, elapsedMs) {
    if (!replayBus?.routes?.length) {
      return null;
    }

    for (let index = replayBus.routes.length - 1; index >= 0; index -= 1) {
      const route = replayBus.routes[index];

      if (elapsedMs >= route.startMs) {
        return route;
      }
    }

    return null;
  }

  function playFoundrySolutionReplay(replayPlan) {
    return new Promise((resolve) => {
      const replayBuses = Array.isArray(replayPlan?.replayBuses) ? replayPlan.replayBuses : [];
      const totalDurationMs = Math.max(Number(replayPlan?.totalDurationMs) || 0, 0);
      state.foundrySolutionReplayResolve = resolve;

      if (!replayBuses.length || !totalDurationMs) {
        clearFoundryAnimationRunners();
        state.foundrySolutionReplayResolve = null;
        resolve();
        return;
      }

      clearFoundryAnimatingBus();
      clearFoundryAnimationRunners();
      beginFoundryReplayStopDemandVisualization();
      const animationStart = performance.now();

      const step = (currentTime) => {
        const elapsed = currentTime - animationStart;
        const cappedElapsed = Math.min(elapsed, totalDurationMs);

        replayBuses.forEach((replayBus) => {
          const activeRoute = getFoundrySolutionReplayRouteAtElapsed(replayBus, cappedElapsed);

          if (!activeRoute) {
            removeFoundryAnimationRunner(replayBus.runnerId);
            return;
          }

          const routeElapsed = clamp(cappedElapsed - activeRoute.startMs, 0, activeRoute.timeline.totalDurationMs);
          const timelineState = getFoundryAnimationTimelineState(activeRoute.timeline, routeElapsed);
          const position = timelineState.position || activeRoute.timeline.lastPosition;

          if (!position) {
            removeFoundryAnimationRunner(replayBus.runnerId);
            return;
          }

          if (timelineState.phase?.type === "pause" && timelineState.phase.pickupDemandLabel) {
            applyFoundryReplayPickupVisualization(
              `${replayBus.runnerId}:${activeRoute.startMs}:${timelineState.phaseIndex}`,
              timelineState.phase.pickupDemandLabel,
              timelineState.phase.boardedWorkers,
            );
          }

          renderFoundryAnimatingBus(position, replayBus.busType, replayBus.runnerId);
        });

        if (cappedElapsed >= totalDurationMs) {
          state.foundrySolutionReplayFrame = 0;
          state.foundrySolutionReplayResolve = null;
          resolve();
          return;
        }

        state.foundrySolutionReplayFrame = window.requestAnimationFrame(step);
      };

      state.foundrySolutionReplayFrame = window.requestAnimationFrame(step);
    });
  }

  function stopFoundrySolutionReplay() {
    if (state.foundrySolutionReplayFrame) {
      window.cancelAnimationFrame(state.foundrySolutionReplayFrame);
      state.foundrySolutionReplayFrame = 0;
    }

    if (typeof state.foundrySolutionReplayResolve === "function") {
      const resolveReplay = state.foundrySolutionReplayResolve;
      state.foundrySolutionReplayResolve = null;
      resolveReplay();
    }

    state.isFoundrySolutionReplayActive = false;
    dom.gameScreen?.classList.remove("is-solution-replay-active");
    clearFoundryAnimationRunners();
    endFoundryReplayStopDemandVisualization();
  }

  function completeFoundryRouteAnimation(busId) {
    const bus = state.purchasedFleet.find((fleetBus) => fleetBus.id === busId);

    clearFoundryAnimatingBus();

    if (bus) {
      const committedRouteLabels = [...getFleetBusRouteNodeLabels(bus)];
      const committedRouteMinutes = calculateFoundryRouteMinutes(committedRouteLabels);
      const committedDutyStartMinutes = Number.isFinite(bus.committedDutyTimeMinutes) ? bus.committedDutyTimeMinutes : 0;
      const draftStopCounts = { ...(bus.draftStopCounts || {}) };
      const draftPickupPlan = { ...(bus.draftPickupPlan || {}) };
      const completedRouteOpportunityCost = calculateCompletedRouteOpportunityCost(bus);

      if (committedRouteLabels.length >= 2 && committedRouteMinutes > 0) {
        bus.completedRouteHistory = [
          ...getFleetBusCompletedRouteHistory(bus),
          {
            routeNodeLabels: committedRouteLabels,
            startMinute: committedDutyStartMinutes,
            durationMinutes: committedRouteMinutes,
            pickupPlan: draftPickupPlan,
            stopCounts: draftStopCounts,
          },
        ];
      }

      bus.servedStopCounts = mergeFoundryStopCounts(
        { ...(bus.servedStopCounts || {}) },
        draftStopCounts,
      );
      bus.totalCost = roundCurrencyAmount((Number(bus.totalCost) || 0) + completedRouteOpportunityCost);
      bus.draftStopCounts = {};
      bus.draftPickupPlan = {};
      bus.committedDutyTimeMinutes = (Number.isFinite(bus.committedDutyTimeMinutes) ? bus.committedDutyTimeMinutes : 0) + getFleetBusDraftRouteMinutes(bus);
      bus.routeNodeLabels = [];
      refreshFleetBusTiming(bus);
    }

    syncSelectedFoundryNodeLabel();
    renderFoundryRoads();
    updateSelectedBusStat();
    updateFoundryDeleteCanState();
    saveFoundryScheduleReplaySnapshot();
  }

  function animateFoundryBusRoute(bus) {
    if (!bus || isFoundryRouteAnimationActive()) {
      return;
    }

    const animationTimeline = buildFoundryRouteAnimationTimeline(bus);

    if (!animationTimeline.totalDurationMs || !animationTimeline.lastPosition) {
      completeFoundryRouteAnimation(bus.id);
      return;
    }

    clearFoundryAnimatingBus();
    state.foundryAnimatingBusId = bus.id;
    renderFoundryRoads();
    updateFoundryDeleteCanState();
    const animationStart = performance.now();
    const initialPosition = getFoundryAnimationTimelinePosition(animationTimeline, 0);

    if (initialPosition) {
      renderFoundryAnimatingBus(initialPosition, bus.type);
    }

    function step(currentTime) {
      const elapsed = currentTime - animationStart;
      const cappedElapsed = Math.min(elapsed, animationTimeline.totalDurationMs);
      const currentPosition = getFoundryAnimationTimelinePosition(animationTimeline, cappedElapsed);

      renderFoundryAnimatingBus(currentPosition, bus.type);

      if (cappedElapsed >= animationTimeline.totalDurationMs) {
        completeFoundryRouteAnimation(bus.id);
        return;
      }

      state.foundryAnimationFrame = window.requestAnimationFrame(step);
    }

    state.foundryAnimationFrame = window.requestAnimationFrame(step);
  }

  function isFoundryRouteClosed(bus) {
    const routeNodeLabels = getFleetBusRouteNodeLabels(bus);

    return (
      routeNodeLabels.length >= 3 &&
      routeNodeLabels[0] === FOUNDRY_FACTORY_START_LABEL &&
      routeNodeLabels[routeNodeLabels.length - 1] === FOUNDRY_FACTORY_START_LABEL
    );
  }

  function renderFoundryFleetYard() {
    const yard = dom.foundryFleetYard;
    const stage = dom.foundryMapStage;
    const visibleFleet = state.purchasedFleet.filter((bus) => bus.id !== state.foundryAnimatingBusId);

    if (!yard) {
      return;
    }

    if (!visibleFleet.length) {
      yard.hidden = true;
      yard.innerHTML = "";
      return;
    }

    if (!stage) {
      return;
    }

    const stageRect = stage.getBoundingClientRect();

    if (!stageRect.width || !stageRect.height) {
      return;
    }

    const yardLeft = stageRect.width * 0.28;
    const yardTop = -4;
    const yardWidth = Math.min(stageRect.width * 0.56, stageRect.width - yardLeft - 28);
    const laneWidths = [yardWidth * 0.42, yardWidth * 0.58, yardWidth];
    const laneTops = [76, 42, 8];
    const laneGap = 6;
    let currentLane = 0;
    const laneOffsets = [0, 0, 0];

    yard.hidden = false;
    yard.style.left = `${yardLeft}px`;
    yard.style.top = `${yardTop}px`;
    yard.style.width = `${yardWidth}px`;
    yard.style.transform = "none";
    yard.innerHTML = visibleFleet.map((bus) => `
      <button
        class="map-fleet-yard__bus${bus.id === state.selectedFleetBusId ? " is-selected" : ""}"
        type="button"
        data-fleet-bus-id="${escapeHtml(bus.id)}"
        aria-label="Select ${escapeHtml(bus.displayName)}"
      >
        <span class="map-fleet-yard__bus-badge">${escapeHtml(bus.badgeLabel)}</span>
        <span class="map-fleet-yard__bus-visual">
          <span class="map-bus map-bus--${escapeHtml(bus.type)}" aria-hidden="true">
            <span class="map-bus__window"></span>
            <span class="map-bus__mark"></span>
          </span>
        </span>
      </button>
    `).join("");

    Array.from(yard.querySelectorAll(".map-fleet-yard__bus")).forEach((busElement) => {
      const busWidth = busElement.offsetWidth;

      while (
        currentLane < laneWidths.length - 1 &&
        laneOffsets[currentLane] > 0 &&
        laneOffsets[currentLane] + busWidth > laneWidths[currentLane]
      ) {
        currentLane += 1;
      }

      const activeLane = Math.min(currentLane, laneWidths.length - 1);
      busElement.style.left = `${laneOffsets[activeLane]}px`;
      busElement.style.top = `${laneTops[activeLane]}px`;
      laneOffsets[activeLane] += busWidth + laneGap;
    });
  }

  function purchaseBus(busType) {
    const config = getBusShopConfig(busType);
    const sequence = getEarliestAvailableFleetSequence(busType);
    const displaySequence = formatFleetSequence(sequence);
    const newBus = {
      id: `${busType}-${displaySequence}`,
      sequence,
      type: busType,
      purchaseCost: config.purchaseCost,
      capacity: config.capacity,
      load: 0,
      routeTimeMinutes: 0,
      routeLimitMinutes: config.routeLimitMinutes,
      committedDutyTimeMinutes: 0,
      dutyTimeMinutes: 0,
      dutyLimitMinutes: config.dutyLimitMinutes,
      totalCost: config.purchaseCost,
      routeNodeLabels: [],
      servedStopCounts: {},
      draftStopCounts: {},
      draftPickupPlan: {},
      completedRouteHistory: [],
      displayName: `${config.name} ${displaySequence}`,
      badgeLabel: `${config.badge}${sequence}`,
    };

    refreshFleetBusTiming(newBus);
    state.purchasedFleet.push(newBus);
    state.selectedFleetBusId = newBus.id;
    syncSelectedFoundryNodeLabel();
    renderFoundryRoads();
    updateSelectedBusStat();
    updateFoundryDeleteCanState();
    saveFoundryScheduleReplaySnapshot();
  }

  function selectFleetBus(busId) {
    const selectedBus = state.purchasedFleet.find((bus) => bus.id === busId);

    if (!selectedBus) {
      return;
    }

    state.selectedFleetBusId = selectedBus.id;
    syncSelectedFoundryNodeLabel();
    renderFoundryRoads();
    updateSelectedBusStat();
    updateFoundryDeleteCanState();
  }

  function deleteSelectedFleetBus() {
    const selectedIndex = state.purchasedFleet.findIndex((bus) => bus.id === state.selectedFleetBusId);

    if (selectedIndex < 0) {
      return;
    }

    state.purchasedFleet.splice(selectedIndex, 1);
    const replacementBus = state.purchasedFleet[selectedIndex] || state.purchasedFleet[selectedIndex - 1] || null;
    state.selectedFleetBusId = replacementBus?.id || "";
    syncSelectedFoundryNodeLabel();
    renderFoundryRoads();
    updateSelectedBusStat();
    updateFoundryDeleteCanState();
    saveFoundryScheduleReplaySnapshot();
  }

  function measureFoundryNodePercent(node) {
    const stage = dom.foundryMapStage;

    if (!stage || !node) {
      return { left: 0, top: 0 };
    }

    const stageRect = stage.getBoundingClientRect();
    const nodeRect = node.getBoundingClientRect();

    return {
      left: roundPercent(((nodeRect.left - stageRect.left + nodeRect.width / 2) / stageRect.width) * 100),
      top: roundPercent(((nodeRect.top - stageRect.top + nodeRect.height / 2) / stageRect.height) * 100),
    };
  }

  function getFoundryStagePointerPlacement(clientX, clientY) {
    const stage = dom.foundryMapStage;
    const stageRect = stage?.getBoundingClientRect();

    if (!stageRect?.width || !stageRect?.height) {
      return { left: 0, top: 0 };
    }

    return {
      left: roundPercent(clamp(((clientX - stageRect.left) / stageRect.width) * 100, 0, 100)),
      top: roundPercent(clamp(((clientY - stageRect.top) / stageRect.height) * 100, 0, 100)),
    };
  }

  function isPointerInsideFoundryStage(clientX, clientY) {
    const stageRect = dom.foundryMapStage?.getBoundingClientRect();

    if (!stageRect?.width || !stageRect?.height) {
      return false;
    }

    return (
      clientX >= stageRect.left &&
      clientX <= stageRect.right &&
      clientY >= stageRect.top &&
      clientY <= stageRect.bottom
    );
  }

  function isPointerOverFoundryDeleteCan(clientX, clientY) {
    const deleteCanRect = dom.foundryDeleteCan?.getBoundingClientRect();

    if (!deleteCanRect?.width || !deleteCanRect?.height) {
      return false;
    }

    return (
      clientX >= deleteCanRect.left &&
      clientX <= deleteCanRect.right &&
      clientY >= deleteCanRect.top &&
      clientY <= deleteCanRect.bottom
    );
  }

  function setFoundryDeleteCanDragState(isArmed) {
    if (!dom.foundryDeleteCan) {
      return;
    }

    dom.foundryDeleteCan.classList.toggle("is-builder-armed", Boolean(isArmed));
  }

  function applyFoundryCampusAssetPlacement(asset, placement) {
    if (!asset || !placement) {
      return;
    }

    if (typeof placement.left === "number") {
      asset.style.left = `${placement.left}%`;
    }

    if (typeof placement.top === "number") {
      asset.style.top = `${placement.top}%`;
    }

    const rotation = typeof placement.rotation === "number" ? placement.rotation : 0;
    const scale = typeof placement.scale === "number" ? placement.scale : 1;
    asset.dataset.rotation = String(rotation);
    asset.dataset.scale = String(scale);
    asset.style.setProperty("--campus-rotation", `${rotation}deg`);
    asset.style.setProperty("--campus-scale", `${scale}`);
  }

  function loadFoundryCampusAssets() {
    if (!dom.foundryCampusAssets?.length) {
      return;
    }

    dom.foundryCampusAssets.forEach((asset) => {
      const assetKey = asset.dataset.campusAsset || "";
      const placement = FOUNDRY_FALLBACK_CAMPUS_ASSETS[assetKey];
      applyFoundryCampusAssetPlacement(asset, placement);
    });
  }

  function measureFoundryCampusAssetPercent(asset) {
    const stage = dom.foundryMapStage;

    if (!stage || !asset) {
      return { left: 0, top: 0 };
    }

    const stageRect = stage.getBoundingClientRect();
    const assetRect = asset.getBoundingClientRect();

    return {
      left: roundPercent(((assetRect.left - stageRect.left + assetRect.width / 2) / stageRect.width) * 100),
      top: roundPercent(((assetRect.top - stageRect.top + assetRect.height / 2) / stageRect.height) * 100),
    };
  }

  function measureFoundryCampusAssetCenter(asset) {
    const stage = dom.foundryMapStage;

    if (!stage || !asset) {
      return { x: 0, y: 0 };
    }

    const stageRect = stage.getBoundingClientRect();
    const assetRect = asset.getBoundingClientRect();

    return {
      x: assetRect.left - stageRect.left + assetRect.width / 2,
      y: assetRect.top - stageRect.top + assetRect.height / 2,
    };
  }

  function projectPointOntoFoundrySegment(point, start, end) {
    const deltaX = end.x - start.x;
    const deltaY = end.y - start.y;
    const segmentLengthSquared = deltaX ** 2 + deltaY ** 2;

    if (!segmentLengthSquared) {
      return { x: start.x, y: start.y, t: 0 };
    }

    const projectionT = clamp(
      (((point.x - start.x) * deltaX) + ((point.y - start.y) * deltaY)) / segmentLengthSquared,
      0,
      1,
    );

    return {
      x: start.x + deltaX * projectionT,
      y: start.y + deltaY * projectionT,
      t: projectionT,
    };
  }

  function positionFoundryPickupNodes() {
    const stage = dom.foundryMapStage;
    const stageRect = stage?.getBoundingClientRect();
    const pickupLayouts = {};

    if (!stage || !stageRect?.width || !stageRect?.height) {
      state.foundryPickupLayouts = pickupLayouts;
      return;
    }

    dom.foundryPickupNodes?.forEach((pickupNode) => {
      const pickupLabel = pickupNode.dataset.routeNodeLabel || "";
      const pickupConfig = getFoundryPickupConfig(pickupLabel);

      if (!pickupLabel || !pickupConfig) {
        pickupNode.hidden = true;
        return;
      }

      const anchorPoint = {
        x: (roundPercent(clamp(Number(pickupConfig.anchorLeft) || 0, 0, 100)) / 100) * stageRect.width,
        y: (roundPercent(clamp(Number(pickupConfig.anchorTop) || 0, 0, 100)) / 100) * stageRect.height,
      };
      let bestPlacement = null;

      state.foundryRoads.forEach((road) => {
        const fromNode = getFoundryNodeByLabel(road.from);
        const toNode = getFoundryNodeByLabel(road.to);

        if (!fromNode || !toNode || isPerimeterFoundryNode(fromNode) || isPerimeterFoundryNode(toNode)) {
          return;
        }

        const fromCenter = measureFoundryNodeCenter(fromNode);
        const toCenter = measureFoundryNodeCenter(toNode);
        const projection = projectPointOntoFoundrySegment(anchorPoint, fromCenter, toCenter);
        const distance = Math.hypot(anchorPoint.x - projection.x, anchorPoint.y - projection.y);

        if (!bestPlacement || distance < bestPlacement.distance) {
          bestPlacement = {
            x: projection.x,
            y: projection.y,
            from: road.from,
            to: road.to,
            distance,
          };
        }
      });

      if (!bestPlacement) {
        pickupNode.hidden = true;
        return;
      }

      pickupNode.hidden = false;
      pickupNode.style.left = `${bestPlacement.x}px`;
      pickupNode.style.top = `${bestPlacement.y}px`;
      pickupLayouts[pickupLabel] = {
        x: bestPlacement.x,
        y: bestPlacement.y,
        from: String(bestPlacement.from),
        to: String(bestPlacement.to),
      };
    });

    state.foundryPickupLayouts = pickupLayouts;
  }

  function saveFoundryCampusAssetPlacement(asset) {
    // Campus assets are hard-coded in project state now.
  }

  function saveFoundryNodePosition(node) {
    // Node positions persist in FOUNDRY_FALLBACK_NODE_POSITIONS at code level.
    // This saves a runtime snapshot to localStorage for export.
    try {
      const positions = {};
      dom.foundryMapNodes?.forEach((n) => {
        const label = n.dataset.label || "";
        const left = parseFloat(n.style.left);
        const top = parseFloat(n.style.top);
        if (label && Number.isFinite(left) && Number.isFinite(top)) {
          positions[label] = { top: roundPercent(top), left: roundPercent(left) };
        }
      });
      localStorage.setItem("routecraft-nodes", JSON.stringify(positions));
    } catch (_) { /* ignore */ }
  }

  function clearFoundryNodeSelection() {
    dom.foundryMapNodes?.forEach((node) => {
      node.classList.remove("map-node--selected");
    });
    dom.foundryPickupNodes?.forEach((node) => {
      node.classList.remove("map-node--selected");
    });
    dom.foundryFactoryStartNode?.classList.remove("map-roundabout__node--selected");
    state.selectedFoundryNodeLabel = "";
  }

  function selectFoundryNode(nodeOrLabel) {
    clearFoundryNodeSelection();
    const label = typeof nodeOrLabel === "string" ? nodeOrLabel : getFoundryRouteNodeLabel(nodeOrLabel);
    const routeNode = getFoundryRouteNodeElement(label);

    if (!routeNode) {
      return;
    }

    routeNode.classList.add(isFactoryFoundryRouteLabel(label) ? "map-roundabout__node--selected" : "map-node--selected");
    state.selectedFoundryNodeLabel = label;
  }

  function clearFoundryReachableNodeState() {
    dom.foundryMapNodes?.forEach((node) => {
      node.classList.remove("map-node--reachable");
    });
    dom.foundryPickupNodes?.forEach((node) => {
      node.classList.remove("map-node--reachable");
    });
    dom.foundryFactoryStartNode?.classList.remove("map-roundabout__node--reachable");
  }

  function appendSelectedBusRouteNode(label) {
    const selectedBus = getSelectedFleetBus();
    const nextLabel = String(label || "");

    if (!selectedBus || !nextLabel || isFoundryRouteAnimationActive()) {
      return;
    }

    const reachableLabels = getReachableFoundryRouteLabels(selectedBus);

    if (!reachableLabels.includes(nextLabel)) {
      return;
    }

    selectedBus.routeNodeLabels = [...getFleetBusRouteNodeLabels(selectedBus), nextLabel];
    refreshFleetBusTiming(selectedBus);
    syncSelectedFoundryNodeLabel();
    renderFoundryRoads();
    updateSelectedBusStat();

    if (isFoundryRouteClosed(selectedBus)) {
      animateFoundryBusRoute(selectedBus);
    }
  }

  function undoSelectedBusRouteNode() {
    const selectedBus = getSelectedFleetBus();

    if (!selectedBus || isFoundryRouteAnimationActive()) {
      return;
    }

    const nextRouteNodeLabels = getFleetBusRouteNodeLabels(selectedBus).slice(0, -1);
    selectedBus.routeNodeLabels = nextRouteNodeLabels;
    refreshFleetBusTiming(selectedBus);
    syncSelectedFoundryNodeLabel();
    renderFoundryRoads();
    updateSelectedBusStat();
  }

  function clearSelectedBusRoute() {
    const selectedBus = getSelectedFleetBus();

    if (!selectedBus || isFoundryRouteAnimationActive()) {
      return;
    }

    selectedBus.routeNodeLabels = [];
    refreshFleetBusTiming(selectedBus);
    syncSelectedFoundryNodeLabel();
    renderFoundryRoads();
    updateSelectedBusStat();
  }

  function saveFoundryRoads() {
    saveFoundryEditLayoutSnapshot();
  }

  function updateFoundryNodeVisibility() {
    const connectedLabels = new Set();

    state.foundryRoads.forEach((road) => {
      connectedLabels.add(String(road.from));
      connectedLabels.add(String(road.to));
    });

    dom.foundryMapNodes.forEach((node) => {
      const label = node.dataset.label || "";
      const isBuilderNode = node.dataset.builderNode === "true";
      const shouldHide = !isBuilderNode && (isPerimeterFoundryNode(node) || !connectedLabels.has(label));

      node.classList.toggle("map-node--concealed", shouldHide);
    });
  }

  function loadFoundryRoads() {
    const sourceRoads = Array.isArray(state.foundryLoadedEditLayout?.roads) && state.foundryLoadedEditLayout.roads.length
      ? state.foundryLoadedEditLayout.roads
      : FOUNDRY_FALLBACK_ROADS;

    const uniqueRoads = new Map();

    sourceRoads.forEach((road) => {
      const from = String(road?.from || "");
      const to = String(road?.to || "");

      if (!getFoundryNodeByLabel(from) || !getFoundryNodeByLabel(to) || from === to) {
        return;
      }

      const roadKey = getFoundryRoadKey(from, to);
      const [sortedFrom, sortedTo] = roadKey.split(":");
      uniqueRoads.set(roadKey, { from: sortedFrom, to: sortedTo });
    });

    state.foundryRoads = Array.from(uniqueRoads.values());
  }

  function renderFoundryRoutePaths(roadLayer) {
    state.purchasedFleet.forEach((bus) => {
      const routeNodeLabels = getFleetBusRouteNodeLabels(bus);
      const isActiveBus = bus.id === state.selectedFleetBusId;

      if (routeNodeLabels.length < 2) {
        return;
      }

      appendFoundryRoutePolyline(roadLayer, buildFoundryRoutePolylinePoints(routeNodeLabels), isActiveBus);
    });
  }

  function renderFoundryRouteNodeState() {
    clearFoundryNodeSelection();
    clearFoundryReachableNodeState();
    
    if (isFoundryRouteAnimationActive()) {
      return;
    }

    syncSelectedFoundryNodeLabel();

    const selectedBus = getSelectedFleetBus();

    if (!selectedBus) {
      return;
    }

    if (state.selectedFoundryNodeLabel) {
      selectFoundryNode(state.selectedFoundryNodeLabel);
    }

    getReachableFoundryRouteLabels(selectedBus).forEach((label) => {
      const routeNode = getFoundryRouteNodeElement(label);

      if (!routeNode) {
        return;
      }

      routeNode.classList.add(isFactoryFoundryRouteLabel(label) ? "map-roundabout__node--reachable" : "map-node--reachable");
    });
  }

  function renderFoundryRoads() {
    const stage = dom.foundryMapStage;
    const roadLayer = dom.foundryRoadLayer;

    if (!stage || !roadLayer) {
      return;
    }

    const stageRect = stage.getBoundingClientRect();

    if (!stageRect.width || !stageRect.height) {
      return;
    }

    renderFoundryNorthwestZone(stageRect);
    renderFoundryFactoryRoundabout();
    positionFoundryPickupNodes();
    renderFoundryBuilderDemandState();
    renderFoundryFleetYard();
    roadLayer.innerHTML = "";

    state.foundryRoads.forEach((road) => {
      const fromNode = getFoundryNodeByLabel(road.from);
      const toNode = getFoundryNodeByLabel(road.to);

      if (!fromNode || !toNode) {
        return;
      }

      const fromCenter = measureFoundryNodeCenter(fromNode);
      const toCenter = measureFoundryNodeCenter(toNode);
      const start = resolveFoundryRoadEndpoint(fromNode, toCenter, stageRect);
      const end = resolveFoundryRoadEndpoint(toNode, fromCenter, stageRect);
      const deltaX = end.x - start.x;
      const deltaY = end.y - start.y;
      const distance = Math.hypot(deltaX, deltaY);
      const angle = Math.atan2(deltaY, deltaX) * (180 / Math.PI);
      const roadElement = document.createElement("span");

      roadElement.className = "map-road-link";
      roadElement.dataset.roadKey = getFoundryRoadKey(road.from, road.to);
      roadElement.style.left = `${start.x}px`;
      roadElement.style.top = `${start.y}px`;
      roadElement.style.width = `${distance}px`;
      roadElement.style.transform = `translateY(-50%) rotate(${angle}deg)`;

      roadLayer.append(roadElement);
    });

    updateFoundryNodeVisibility();
    renderFoundryRoutePaths(roadLayer);
    renderFoundryRouteNodeState();
  }

  function loadFoundryLevelMapState() {
    if (!dom.foundryMapStage) {
      return;
    }

    loadFoundryNodePositions();
    loadFoundryCampusAssets();
    loadFoundryEditLayout();
    applyFoundryLegacyOverridesToDom();
    loadFoundryRoads();
    state.foundryStopDemand = createFoundryStopDemandSnapshot();

    clearFoundryNodeSelection();
    clearFoundryBuilderAssetSelection();
    refreshFoundryMapCollections();
    renderFoundryStopDemandState();
    renderFoundryRoads();
  }

  function bindFoundryRoadBuilder() {
    const stage = dom.foundryMapStage;

    if (!stage || !dom.foundryMapNodes?.length) {
      return;
    }

    stage.addEventListener("click", (event) => {
      if (!state.isEditMode) {
        return;
      }

      if (state.suppressNextFoundryRoadClick) {
        state.suppressNextFoundryRoadClick = false;
        return;
      }

      const road = event.target.closest(".map-road-link");

      if (road && stage.contains(road)) {
        const roadKey = road.dataset.roadKey || "";

        state.foundryRoads = state.foundryRoads.filter(
          (existingRoad) => getFoundryRoadKey(existingRoad.from, existingRoad.to) !== roadKey,
        );
        saveFoundryRoads();
        clearFoundryNodeSelection();
        renderFoundryRoads();
        return;
      }

      const node = event.target.closest(".map-node");

      if (!node || !stage.contains(node)) {
        clearFoundryBuilderAssetSelection();
        return;
      }

      clearFoundryBuilderAssetSelection();

      const selectedLabel = state.selectedFoundryNodeLabel;
      const currentLabel = node.dataset.label || "";

      if (!selectedLabel) {
        selectFoundryNode(node);
        return;
      }

      if (selectedLabel === currentLabel) {
        if (node.dataset.builderNodeVariant === "blue") {
          promptForFoundryBuilderNodeDemand(currentLabel);
        }

        clearFoundryNodeSelection();
        return;
      }

      const validation = validateFoundryEditorRoadAddition(selectedLabel, currentLabel);

      if (!validation.isAllowed) {
        showToast(validation.message, {
          title: validation.title,
        });
        return;
      }

      const [from, to] = getFoundryRoadKey(selectedLabel, currentLabel).split(":");
      state.foundryRoads.push({ from, to });
      collapseFoundryInlineBlueDirectRoad(from);
      collapseFoundryInlineBlueDirectRoad(to);
      saveFoundryRoads();
      renderFoundryRoads();

      clearFoundryNodeSelection();
    });

    window.addEventListener("resize", renderFoundryRoads);

    window.RouteCraftFoundryRoads = {
      getRoads() {
        return state.foundryRoads.map((road) => ({ ...road }));
      },
      reset() {
        state.foundryRoads = [];
        saveFoundryRoads();
        clearFoundryNodeSelection();
        renderFoundryRoads();
      },
      exportState() {
        const positions = {};
        dom.foundryMapNodes?.forEach((n) => {
          const label = n.dataset.label || "";
          const left = parseFloat(n.style.left);
          const top = parseFloat(n.style.top);
          if (label && Number.isFinite(left) && Number.isFinite(top)) {
            positions[label] = { top: roundPercent(top), left: roundPercent(left) };
          }
        });
        const output = {
          levelName: state.currentLevel,
          nodes: positions,
          roads: state.foundryRoads.map((r) => ({ from: r.from, to: r.to })),
          customNodes: state.foundryCustomNodes.map((node) => ({ ...node })),
          customAssets: [],
        };
        console.log(JSON.stringify(output));
        return output;
      },
    };
  }

  function bindFoundryPerimeterNodeDragging() {
    const stage = dom.foundryMapStage;

    if (!stage || !dom.foundryMapNodes?.length) {
      return;
    }

    let dragState = null;

    function updateDraggedNodePosition(event) {
      if (!dragState) {
        return;
      }

      const stageRect = stage.getBoundingClientRect();
      const nextLeft = clamp(((event.clientX - stageRect.left) / stageRect.width) * 100, 0, 100);
      const nextTop = clamp(((event.clientY - stageRect.top) / stageRect.height) * 100, 0, 100);

      dragState.node.style.left = `${roundPercent(nextLeft)}%`;
      dragState.node.style.top = `${roundPercent(nextTop)}%`;
      renderFoundryRoads();
    }

    function stopDragging(event) {
      if (!dragState || event.pointerId !== dragState.pointerId) {
        return;
      }

      dragState.node.releasePointerCapture?.(dragState.pointerId);
      dragState.node.classList.remove("map-node--perimeter-dragging");

      if (dragState.moved) {
        state.suppressNextFoundryRoadClick = true;
        saveFoundryNodePosition(dragState.node);
      }

      dragState = null;
    }

    stage.addEventListener("pointerdown", (event) => {
      if (state.isEditMode) {
        return;
      }

      const node = event.target.closest(".map-node");

      if (!node || !stage.contains(node) || !isPerimeterFoundryNode(node)) {
        return;
      }

      dragState = {
        node,
        pointerId: event.pointerId,
        startX: event.clientX,
        startY: event.clientY,
        moved: false,
      };

      node.classList.add("map-node--perimeter-dragging");
      node.setPointerCapture?.(event.pointerId);
      event.preventDefault();
    });

    stage.addEventListener("pointermove", (event) => {
      if (!dragState || event.pointerId !== dragState.pointerId) {
        return;
      }

      const traveled = Math.hypot(event.clientX - dragState.startX, event.clientY - dragState.startY);

      if (traveled > 4 && !dragState.moved) {
        dragState.moved = true;
        clearFoundryNodeSelection();
      }

      if (!dragState.moved) {
        return;
      }

      event.preventDefault();
      updateDraggedNodePosition(event);
    });

    stage.addEventListener("pointerup", stopDragging);
    stage.addEventListener("pointercancel", stopDragging);
  }

  function bindFoundryBuilderItemDragging() {
    const stage = dom.foundryMapStage;

    if (!stage) {
      return;
    }

    let dragState = null;

    function applyDraggedPlacement(placement) {
      if (!dragState) {
        return;
      }

      if (dragState.kind === "node") {
        dragState.element.style.left = `${placement.left}%`;
        dragState.element.style.top = `${placement.top}%`;
        renderFoundryRoads();
        return;
      }

      const rotation = Number(dragState.element.dataset.rotation) || 0;
      const scale = Number(dragState.element.dataset.scale) || 1;
      applyFoundryCampusAssetPlacement(dragState.element, {
        left: placement.left,
        top: placement.top,
        rotation,
        scale,
      });

      if (!dragState.isLegacy) {
        const draggingAssetRecord = state.foundryCustomAssets.find(
          (assetRecord) => assetRecord.id === String(dragState.element.dataset.campusAsset),
        );

        if (draggingAssetRecord?.pairedNodeLabel) {
          draggingAssetRecord.left = roundPercent(clamp(placement.left, 0, 100));
          draggingAssetRecord.top = roundPercent(clamp(placement.top, 0, 100));
          syncFoundryBuilderPairedBlueNodeToAsset(draggingAssetRecord);
          renderFoundryRoads();
        }
      }
    }

    function stopDragging(event) {
      if (!dragState || event.pointerId !== dragState.pointerId) {
        return;
      }

      const droppedOnDeleteCan = dragState.moved && isPointerOverFoundryDeleteCan(event.clientX, event.clientY);
      dragState.element.releasePointerCapture?.(dragState.pointerId);
      dragState.element.classList.remove("map-node--perimeter-dragging", "lmsa-campus-asset--dragging");
      setFoundryDeleteCanDragState(false);

      if (dragState.moved) {
        state.suppressNextFoundryRoadClick = true;

        if (droppedOnDeleteCan) {
          if (dragState.isLegacy) {
            if (dragState.kind === "node") {
              removeLegacyFoundryNode(dragState.element.dataset.label);
            } else {
              removeLegacyFoundryAsset(dragState.element.dataset.campusAsset);
            }
          } else if (dragState.kind === "node") {
            removeFoundryBuilderNode(dragState.element.dataset.label);
          } else {
            removeFoundryBuilderAsset(dragState.element.dataset.campusAsset);
          }
        } else if (dragState.isLegacy) {
          if (dragState.kind === "node") {
            const measuredNodePosition = measureFoundryNodePercent(dragState.element);
            updateLegacyFoundryNodePosition(dragState.element.dataset.label, measuredNodePosition);
            dragState.element.style.left = `${measuredNodePosition.left}%`;
            dragState.element.style.top = `${measuredNodePosition.top}%`;
            saveFoundryNodePosition(dragState.element);
          } else {
            const measuredAssetPosition = measureFoundryCampusAssetPercent(dragState.element);
            const existingRotation = Number(dragState.element.dataset.rotation) || 0;
            const existingScale = Number(dragState.element.dataset.scale) || 1;
            const updatedPlacement = {
              ...measuredAssetPosition,
              rotation: existingRotation,
              scale: existingScale,
            };
            updateLegacyFoundryAssetPlacement(dragState.element.dataset.campusAsset, updatedPlacement);
            applyFoundryCampusAssetPlacement(dragState.element, updatedPlacement);
          }
        } else if (dragState.kind === "node") {
          const updatedNodeRecord = updateFoundryBuilderNodePosition(
            dragState.element.dataset.label,
            measureFoundryNodePercent(dragState.element),
          );

          if (updatedNodeRecord) {
            syncFoundryBuilderNodeElement(updatedNodeRecord);
          }
        } else {
          const updatedAssetRecord = updateFoundryBuilderAssetPlacement(
            dragState.element.dataset.campusAsset,
            {
              ...measureFoundryCampusAssetPercent(dragState.element),
              rotation: Number(dragState.element.dataset.rotation) || 0,
              scale: Number(dragState.element.dataset.scale) || FOUNDRY_DEFAULT_BUILDER_STOP_SIGN_SCALE,
            },
          );

          if (updatedAssetRecord) {
            applyFoundryCampusAssetPlacement(dragState.element, updatedAssetRecord);
            syncFoundryBuilderPairedBlueNodeToAsset(updatedAssetRecord);
          }
        }

        renderFoundryRoads();
        saveFoundryEditLayoutSnapshot();
      }

      dragState = null;
    }

    stage.addEventListener("pointerdown", (event) => {
      if (!state.isEditMode) {
        return;
      }

      const builderNode = event.target.closest(".map-node[data-builder-node=\"true\"]");
      const builderAsset = event.target.closest(".lmsa-campus-asset[data-builder-asset=\"true\"]");
      const legacyNode = !builderNode && !builderAsset
        ? event.target.closest(".map-node")
        : null;
      const legacyAsset = !builderNode && !builderAsset && !legacyNode
        ? event.target.closest(".lmsa-campus-asset")
        : null;
      const legacyNodeIsEditable = legacyNode && isLegacyFoundryNodeElement(legacyNode);
      const legacyAssetIsEditable = legacyAsset && isLegacyFoundryAssetElement(legacyAsset);
      const dragElement = builderNode
        || builderAsset
        || (legacyNodeIsEditable ? legacyNode : null)
        || (legacyAssetIsEditable ? legacyAsset : null);

      if (!dragElement || !stage.contains(dragElement)) {
        return;
      }

      const isNodeKind = Boolean(builderNode) || legacyNodeIsEditable;
      const isLegacy = !builderNode && !builderAsset;

      dragState = {
        element: dragElement,
        kind: isNodeKind ? "node" : "asset",
        isLegacy,
        pointerId: event.pointerId,
        startX: event.clientX,
        startY: event.clientY,
        moved: false,
      };

      dragElement.classList.add(isNodeKind ? "map-node--perimeter-dragging" : "lmsa-campus-asset--dragging");
      dragElement.setPointerCapture?.(event.pointerId);
      event.preventDefault();
    });

    stage.addEventListener("pointermove", (event) => {
      if (!dragState || event.pointerId !== dragState.pointerId) {
        return;
      }

      const traveled = Math.hypot(event.clientX - dragState.startX, event.clientY - dragState.startY);

      if (traveled > 4 && !dragState.moved) {
        dragState.moved = true;
        clearFoundryNodeSelection();
      }

      if (!dragState.moved) {
        return;
      }

      event.preventDefault();
      applyDraggedPlacement(getFoundryStagePointerPlacement(event.clientX, event.clientY));
      setFoundryDeleteCanDragState(isPointerOverFoundryDeleteCan(event.clientX, event.clientY));
    });

    stage.addEventListener("pointerup", stopDragging);
    stage.addEventListener("pointercancel", stopDragging);
  }

  function bindFoundryEditSandlot() {
    const stage = dom.foundryMapStage;
    const sandlot = dom.foundryEditSandlot;

    if (!stage || !sandlot) {
      return;
    }

    let spawnState = null;

    function applySpawnPlacement(placement) {
      if (!spawnState) {
        return;
      }

      spawnState.lastPlacement = placement;
      spawnState.element.style.left = `${placement.left}%`;
      spawnState.element.style.top = `${placement.top}%`;
      renderFoundryRoads();
    }

    function cancelSpawn() {
      if (!spawnState) {
        return;
      }

      removeFoundryBuilderNode(spawnState.record.label);

      renderFoundryRoads();
      saveFoundryEditLayoutSnapshot();
    }

    function stopSpawning(event) {
      if (!spawnState || event.pointerId !== spawnState.pointerId) {
        return;
      }

      spawnState.item.releasePointerCapture?.(spawnState.pointerId);
      spawnState.item.classList.remove("is-dragging");

      if (!spawnState.isInsideStage) {
        cancelSpawn();
        spawnState = null;
        return;
      }

      const updatedNodeRecord = updateFoundryBuilderNodePosition(spawnState.record.label, spawnState.lastPlacement);

      if (updatedNodeRecord) {
        syncFoundryBuilderNodeElement(updatedNodeRecord);
      }

      renderFoundryRoads();
      saveFoundryEditLayoutSnapshot();

      if (spawnState.record.variant === "blue") {
        updateSelectedBusStat();
        promptForFoundryBuilderNodeDemand(spawnState.record.label);
      }

      spawnState = null;
    }

    sandlot.addEventListener("click", (event) => {
      const sandlotItem = event.target.closest("[data-sandlot-spawn]");

      if (!sandlotItem) {
        return;
      }

      event.preventDefault();
      event.stopPropagation();
    });

    sandlot.addEventListener("pointerdown", (event) => {
      if (!state.isEditMode) {
        return;
      }

      const sandlotItem = event.target.closest("[data-sandlot-spawn]");

      if (!sandlotItem) {
        return;
      }

      const placement = getFoundryStagePointerPlacement(event.clientX, event.clientY);
      const spawnType = sandlotItem.dataset.sandlotSpawn || "";
      const createdItem = addFoundryBuilderNode(spawnType === "blue-node" ? "blue" : "brown", placement);

      if (!createdItem?.element || !createdItem?.record) {
        return;
      }

      spawnState = {
        item: sandlotItem,
        element: createdItem.element,
        record: createdItem.record,
        kind: "node",
        pointerId: event.pointerId,
        isInsideStage: isPointerInsideFoundryStage(event.clientX, event.clientY),
        lastPlacement: placement,
      };

      sandlotItem.classList.add("is-dragging");
      sandlotItem.setPointerCapture?.(event.pointerId);
      event.preventDefault();
      event.stopPropagation();
      renderFoundryRoads();
    });

    sandlot.addEventListener("pointermove", (event) => {
      if (!spawnState || event.pointerId !== spawnState.pointerId) {
        return;
      }

      spawnState.isInsideStage = isPointerInsideFoundryStage(event.clientX, event.clientY);
      applySpawnPlacement(getFoundryStagePointerPlacement(event.clientX, event.clientY));
      event.preventDefault();
    });

    sandlot.addEventListener("pointerup", stopSpawning);
    sandlot.addEventListener("pointercancel", stopSpawning);
  }

  function initializeFoundryNodes() {
    loadFoundryLevelMapState();
    bindFoundryRoadBuilder();
    bindFoundryPerimeterNodeDragging();
    bindFoundryBuilderItemDragging();
    bindFoundryEditSandlot();
    window.addEventListener("resize", renderFoundryRoads);

    if (dom.foundryMapStage && typeof ResizeObserver === "function") {
      const stageObserver = new ResizeObserver(() => {
        window.requestAnimationFrame(() => {
          renderFoundryRoads();
        });
      });

      stageObserver.observe(dom.foundryMapStage);
    }

    syncFoundryMapLayout(1);
  }

  function initializeFoundryCampus() {
    syncFoundryMapLayout(1);
    updateSelectedBusStat();
  }

  function applyGameConsoleReferences() {
    const gameScreen = document.getElementById("game");

    if (!gameScreen) {
      return;
    }

    gameScreen.querySelectorAll(".console-ref").forEach((element) => {
      element.classList.remove("console-ref");
      element.removeAttribute("data-ref");
    });

    Array.from(document.querySelectorAll(GAME_CONSOLE_REF_SELECTORS)).forEach((element, index) => {
      if (element.dataset[CONSOLE_REF_HIDDEN_ATTRIBUTE] === "true" || element.hidden) {
        return;
      }

      element.classList.add("console-ref");
      element.dataset.ref = String(index + 1);
    });
  }

  function bindScreenNavigation() {
    document.querySelectorAll("[data-screen-target]").forEach((button) => {
      button.addEventListener("click", () => {
        const targetScreen = button.dataset.screenTarget;
        const focusPanel = button.dataset.focusPanel;

        showScreen(targetScreen);

        if (focusPanel) {
          window.setTimeout(() => {
            highlightPanel(focusPanel);
          }, 180);
        }
      });
    });
  }

  function bindLevelSelection() {
    document.addEventListener("click", (event) => {
      const levelButton = event.target.closest(".level-open, [data-launch-level]");

      if (!levelButton) {
        return;
      }

      const levelName = levelButton.dataset.level || levelButton.dataset.launchLevel || DEFAULT_LEVEL;
      const level = getLevelData(levelName);

      if (isLevelComingSoon(level)) {
        showToast(getLevelLockedToastMessage(level), {
          title: "Coming Soon",
        });
        return;
      }

      launchLevel(levelName);
    });
  }

  function bindFleetShop() {
    document.addEventListener("click", (event) => {
      const buyButton = event.target.closest("[data-buy-bus]");

      if (!buyButton) {
        return;
      }

      purchaseBus(buyButton.dataset.buyBus || "lift");
    });
  }

  function bindFoundryFleetSelection() {
    dom.foundryFleetYard?.addEventListener("click", (event) => {
      const busButton = event.target.closest("[data-fleet-bus-id]");

      if (!busButton) {
        return;
      }

      selectFleetBus(busButton.dataset.fleetBusId || "");
    });
  }

  function bindFoundryDeleteCan() {
    dom.foundryDeleteCan?.addEventListener("click", () => {
      if (state.isEditMode) {
        return;
      }

      deleteSelectedFleetBus();
    });
  }

  function bindFoundryRouteBuilder() {
    dom.foundryMapStage?.addEventListener("click", (event) => {
      if (state.isEditMode) {
        return;
      }

      const routeNode = event.target.closest(".map-node, [data-route-node-label]");

      if (!routeNode || !dom.foundryMapStage?.contains(routeNode)) {
        return;
      }

      const routeNodeLabel = getFoundryRouteNodeLabel(routeNode);

      if (!routeNodeLabel) {
        return;
      }

      if (shouldWarnFoundryRouteLimit(getSelectedFleetBus(), routeNodeLabel)) {
        showToast("That move would exceed this bus's route or duty time limit.", {
          title: "Route Blocked",
        });
        return;
      }

      appendSelectedBusRouteNode(routeNodeLabel);
    });
  }

  function bindFoundryRouteControls() {
    dom.undoRouteButton?.addEventListener("click", undoSelectedBusRouteNode);
    dom.clearBusScheduleButton?.addEventListener("click", clearSelectedBusRoute);
    dom.restartLevelButton?.addEventListener("click", restartCurrentLevel);
  }

  function bindFoundryConfettiEmitterDragging() {
    const celebrationLayer = dom.foundryCelebrationLayer;

    if (!celebrationLayer || !dom.foundryConfettiEmitterHandles?.length) {
      return;
    }

    let dragState = null;

    function updateDraggedEmitter(event) {
      if (!dragState) {
        return;
      }

      const layerRect = celebrationLayer.getBoundingClientRect();

      if (!layerRect.width || !layerRect.height) {
        return;
      }

      const nextLeft = roundPercent(clamp(((event.clientX - layerRect.left) / layerRect.width) * 100, 0, 100));
      const nextTop = roundPercent(clamp(((event.clientY - layerRect.top) / layerRect.height) * 100, 0, 100));
      dragState.handle.style.left = `${nextLeft}%`;
      dragState.handle.style.top = `${nextTop}%`;
    }

    function stopDragging(event) {
      if (!dragState || event.pointerId !== dragState.pointerId) {
        return;
      }

      dragState.handle.releasePointerCapture?.(dragState.pointerId);
      dragState.handle.classList.remove("is-dragging");
      saveFoundryConfettiEmitterPositions();
      dragState = null;
    }

    dom.foundryConfettiEmitterHandles.forEach((handle) => {
      handle.addEventListener("pointerdown", (event) => {
        dragState = {
          handle,
          pointerId: event.pointerId,
        };
        handle.classList.add("is-dragging");
        handle.setPointerCapture?.(event.pointerId);
        event.preventDefault();
      });

      handle.addEventListener("pointermove", (event) => {
        if (!dragState || dragState.handle !== handle || event.pointerId !== dragState.pointerId) {
          return;
        }

        updateDraggedEmitter(event);
      });

      handle.addEventListener("pointerup", stopDragging);
      handle.addEventListener("pointercancel", stopDragging);
    });
  }

  function bindFoundryTestingShortcut() {
    window.addEventListener("keydown", (event) => {
      if (event.key !== "Tab" || state.currentScreen !== "game") {
        return;
      }

      const target = event.target;

      if (
        target instanceof HTMLElement &&
        (target.isContentEditable ||
          target.tagName === "INPUT" ||
          target.tagName === "TEXTAREA" ||
          target.tagName === "SELECT")
      ) {
        return;
      }

      event.preventDefault();
      state.debugForceSubmitReady = true;
      updateSelectedBusStat();
    });

    window.addEventListener("keydown", (event) => {
      if (event.key !== "e" || event.repeat || event.altKey || event.ctrlKey || event.metaKey) {
        return;
      }

      if (state.currentScreen !== "game") {
        return;
      }

      const target = event.target;

      if (
        target instanceof HTMLElement &&
        (target.isContentEditable ||
          target.tagName === "INPUT" ||
          target.tagName === "TEXTAREA" ||
          target.tagName === "SELECT")
      ) {
        return;
      }

      event.preventDefault();
      state.isEditMode = !state.isEditMode;
      document.body.classList.toggle("foundry-edit-mode", state.isEditMode);
      clearFoundryNodeSelection();
      clearFoundryBuilderAssetSelection();
      updateFoundryDeleteCanState();
      renderFoundryRoads();

      showToast(
        state.isEditMode
          ? "Drag any node to move or delete it. Click any two nodes to add a road. Dropping a blue node prompts for demand, and clicking it again lets you edit that demand."
          : "Edit mode disabled. Route building restored.",
        { title: state.isEditMode ? "Edit Mode On" : "Edit Mode Off" },
      );
    });
  }

  function bindGameActions() {
    const startGameButton = document.getElementById("startGameButton");
    const dispatchRouteButton = document.getElementById("dispatchRouteButton");
    const dispatchFleetButton = document.getElementById("dispatchFleetButton");
    const retryLevelButton = document.getElementById("retryLevelButton");

    startGameButton?.addEventListener("click", () => {
      launchLevel(DEFAULT_LEVEL);
    });

    dispatchRouteButton?.addEventListener("click", () => {});

    dispatchFleetButton?.addEventListener("click", () => {
      window.setTimeout(() => {
        showScreen("results");
      }, 220);
    });

    dom.submitSolutionButton?.addEventListener("click", () => {
      if (!isSubmitSolutionReady()) {
        showToast("Serve every customer before submitting the solution.", {
          title: "Not Ready",
        });
        return;
      }

      if (state.isVictoryOverlayVisible || state.victoryOverlayTimeoutId) {
        return;
      }

      saveFoundryScheduleReplaySnapshot();
      saveFoundryScheduleReplaySnapshot(FOUNDRY_SUBMITTED_SCHEDULE_REPLAY_STORAGE_KEY);
      launchFoundryFactoryConfetti();
      scheduleVictoryOverlay();
    });

    retryLevelButton?.addEventListener("click", restartCurrentLevel);

    dom.gameVictoryNextLevelButton?.addEventListener("click", () => {
      const nextLevel = getNextLevelData(state.currentLevel);

      if (!nextLevel) {
        return;
      }

      launchLevel(nextLevel.name);
    });

    dom.gameVictoryReplayButton?.addEventListener("click", restartCurrentLevel);

    dom.gameVictoryLevelSelectButton?.addEventListener("click", () => {
      hideVictoryOverlay();
      showScreen("levels");
    });

    dom.gameVictoryViewSolutionReplayButton?.addEventListener("click", () => {
      void runFoundrySolutionReplayFromVictoryOverlay();
    });

    dom.gameVictoryViewOptimalReplayButton?.addEventListener("click", () => {
      void runFoundryOptimalReplayFromVictoryOverlay();
    });

    window.addEventListener("keydown", (event) => {
      if (
        event.key !== "Enter" ||
        event.repeat ||
        event.altKey ||
        event.ctrlKey ||
        event.metaKey ||
        !state.isVictoryOverlayVisible
      ) {
        return;
      }

      const target = event.target;

      if (
        target instanceof HTMLElement &&
        (target.isContentEditable ||
          target.tagName === "INPUT" ||
          target.tagName === "TEXTAREA" ||
          target.tagName === "SELECT")
      ) {
        return;
      }

      event.preventDefault();
      replayVictoryOverlayAnimations();
    });
  }

  function bindPlaceholderButtons() {
    document.querySelectorAll(".js-placeholder-action").forEach((button) => {
      button.addEventListener("click", () => {});
    });
  }

  function bindSettings() {
    const settingsForm = document.getElementById("settingsForm");
    const resetSettingsButton = document.getElementById("resetSettingsButton");

    settingsForm?.addEventListener("submit", (event) => {
      event.preventDefault();
      showScreen("menu");
    });

    resetSettingsButton?.addEventListener("click", () => {});
  }

  function init() {
    document.body.dataset.theme = "lehigh-university";
    cacheDom();
    renderFeaturedLevel();
    renderLevelGrid();
    setCurrentLevel(state.currentLevel);
    applyGameConsoleReferences();
    bindScreenNavigation();
    bindLevelSelection();
    bindFleetShop();
    bindFoundryFleetSelection();
    bindFoundryDeleteCan();
    bindFoundryRouteBuilder();
    bindFoundryRouteControls();
    // bindFoundryConfettiEmitterDragging();
    bindFoundryTestingShortcut();
    bindGameActions();
    bindPlaceholderButtons();
    bindSettings();
    initializeGuideReplayDemo();
    initializeFoundryNodes();
    initializeFoundryCampus();
    showScreen(state.currentScreen, { suppressScroll: true });
  }

  return { init };
})();

document.addEventListener("DOMContentLoaded", LMSAApp.init);

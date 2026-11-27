#pragma once
// Florentino module, included after existing ActorLinker declarations.
static std::recursive_mutex FloMutex;
static void (*FloMove)(void*, Vector2, Vector2) = nullptr;
static void (*FloStop)(void*) = nullptr;
static void (*FloOriginalUpdate)(void*) = nullptr;
static bool (*FloCanSkill)(void*, int) = nullptr;
static bool (*FloSkill)(void*, int, int, unsigned int, int) = nullptr;
static uintptr_t FloUpdateOffset = 0;
static bool FloResolved = false;
static int FloMode = 0;
struct FloSample { Vector3 pos; double time; };
static std::unordered_map<int, FloSample> FloFlowers, FloEnemies;
static Vector3 FloMyPos;
static int FloCamp = 0, FloHeroId = 0, FloHostObj = 0;
static double FloSeen = 0, FloMoveTime = 0, FloSkillTime = 0;
static bool FloAlive = false, FloMoving = false;
static double FloNow() { return CACurrentMediaTime(); }
static bool FloActorsReady() {
    const uintptr_t base = getRealOffset(0);
    return base && (uintptr_t)ActorLinker_IsHostPlayer != base && (uintptr_t)ActorLinker_IsHostCamp != base &&
        (uintptr_t)ActorLinker_get_playerId != base && (uintptr_t)ActorLinker_get_ObjID != base &&
        (uintptr_t)ActorLinker_getPosition != base && (uintptr_t)ActorLinker_COM_PLAYERCAMP != base &&
        (uintptr_t)ActorLinker_ActorTypeDef != base && (uintptr_t)ValueLinkerComponent_get_actorHp != base &&
        ActorLinker_IsHostPlayer && ActorLinker_IsHostCamp && ActorLinker_get_playerId &&
        ActorLinker_get_ObjID && ActorLinker_getPosition && ActorLinker_COM_PLAYERCAMP &&
        ActorLinker_ActorTypeDef && ValueLinkerComponent_get_actorHp &&
        ActorLinkerOffField[0] && ActorLinkerOffField[3] && ActorLinkerOffField[11];
}
static bool FloReady() { return FloActorsReady() && FloMove && FloStop && FloOriginalUpdate && FloCanSkill && FloSkill; }
static bool FloActive() { return mua && FloReady() && FloHeroId == 521 && FloAlive && FloNow() - FloSeen < 0.5; }
static void FloObserve(void *actor) {
    std::lock_guard<std::recursive_mutex> lock(FloMutex);
    if (!mua || !actor || !FloActorsReady()) return;
    const int type = ActorLinker_ActorTypeDef(actor);
    if (type != 0 && type != 4) return;
    const int id = ActorLinker_get_ObjID(actor);
    if (!id) return;
    const double now = FloNow();
    Vector3 pos = ActorLinker_getPosition(actor);
    if (type == 0) {
        void *vc = *(void **)((uintptr_t)actor + ActorLinkerOffField[0]);
        const int hp = vc ? ValueLinkerComponent_get_actorHp(vc) : 0;
        if (ActorLinker_IsHostPlayer(actor)) {
            void *obj = *(void **)((uintptr_t)actor + ActorLinkerOffField[3]);
            FloHeroId = obj ? *(int*)((uintptr_t)obj + ActorLinkerOffField[11]) : 0;
            FloHostObj = id; FloMyPos = pos; FloSeen = now; FloAlive = hp > 1;
            FloCamp = ActorLinker_COM_PLAYERCAMP(actor);
        } else if (hp > 0 && FloCamp && ActorLinker_COM_PLAYERCAMP(actor) != FloCamp) {
            FloEnemies[id] = {pos, now};
        } else FloEnemies.erase(id);
    } else {
        int pid = ActorLinker_get_playerId(actor);
        float d = Vector3::Distance(FloMyPos, pos);
        if (FloHeroId == 521 && now - FloSeen < 0.5 && ActorLinker_IsHostCamp(actor) &&
            pid != 0 && pid != 10001 && d > 1.8f && d < 12.f && pos.y < 0.3f)
            FloFlowers[id] = {pos, now};
        else FloFlowers.erase(id);
    }
}
static void FloForget(void* actor) {
    std::lock_guard<std::recursive_mutex> lock(FloMutex);
    if (!mua || !actor || !FloActorsReady()) return;
    int id = ActorLinker_get_ObjID(actor);
    FloFlowers.erase(id); FloEnemies.erase(id);
    if (id == FloHostObj) { FloAlive = false; FloHeroId = 0; FloSeen = 0; FloCamp = 0; FloHostObj = 0; }
}
static bool FloNearest(std::unordered_map<int,FloSample>& samples, float lo, float& hi, Vector3& pos) {
    bool found = false; double now = FloNow();
    for (auto it = samples.begin(); it != samples.end();) {
        if (now - it->second.time > 0.5) { it = samples.erase(it); continue; }
        float d = Vector3::Distance(FloMyPos, it->second.pos);
        if (d > lo && d < hi) { hi = d; pos = it->second.pos; found = true; }
        ++it;
    }
    return found;
}
static void FloInputUpdate(void* input) {
    if (FloOriginalUpdate) FloOriginalUpdate(input);
    std::lock_guard<std::recursive_mutex> lock(FloMutex);
    if (!input) return;
    if (!FloActive()) {
        if (FloMoving && FloStop) FloStop(input);
        FloMoving = false; FloFlowers.clear(); FloEnemies.clear(); return;
    }
    Vector3 enemy, flower; float ed = 7.f, fd = 12.f;
    if (!FloNearest(FloEnemies, -1.f, ed, enemy) || !FloNearest(FloFlowers, 1.8f, fd, flower)) {
        if (FloMoving) FloStop(input);
        FloMoving = false; return;
    }
    double now = FloNow(); if (now - FloMoveTime < 0.05) return;
    Vector3 delta = flower - FloMyPos;
    float len = sqrtf(delta.x*delta.x + delta.z*delta.z);
    if (len < 0.001f) return;
    float speed = (FloCamp == 2 ? -1600.f : 1600.f) / len;
    FloMove(input, Vector2(0.f,0.f), Vector2(delta.x*speed,delta.z*speed));
    FloMoving = true; FloMoveTime = now;
}
static void FloUseSkills(void* manager) {
    std::lock_guard<std::recursive_mutex> lock(FloMutex);
    if (!manager || !FloActive()) return;
    double now = FloNow(); if (now - FloSkillTime < 0.1) return;
    Vector3 pos; float ed = 7.f, fd = 12.f;
    if (!FloNearest(FloEnemies,-1.f,ed,pos)) return;
    bool flower = FloNearest(FloFlowers,1.8f,fd,pos);
    int slot = -1;
    if (ed < 1.5f && FloCanSkill(manager,2)) slot=2;
    else if (ed > 2.4f && ed < 5.f && (!FloMode || flower) && FloCanSkill(manager,FloMode ? 3 : 1)) slot=FloMode ? 3 : 1;
    else if (FloCanSkill(manager,0)) slot=0;
    if (slot >= 0) { FloSkillTime = now; FloSkill(manager,slot,0,0,0); }
}
static void FloInit() {
    std::lock_guard<std::recursive_mutex> lock(FloMutex);
    const char* dll = "Project_d.dll";
    const char* logic = "Assets.Scripts.GameLogic";
    const char* sys = "Assets.Scripts.GameSystem";
    auto addr = [&](const char* ns, const char* cl, const char* name, int n, const char* a=nullptr, const char* b=nullptr) -> uintptr_t {
        uintptr_t r = IL2CPP_Bridge::NewGetMethod(dll, ns, cl, name, n, a, b);
        return r ? getRealOffset(r) : 0;
    };
    FloMove = (decltype(FloMove))addr(logic,"GameInput","SendMoveDirection",2,"start","end");
    FloStop = (decltype(FloStop))addr(logic,"GameInput","StopInput",0);
    FloCanSkill = (decltype(FloCanSkill))addr(sys,"CSkillButtonManager","CanRequestSkill",1);
    FloSkill = (decltype(FloSkill))addr(sys,"CSkillButtonManager","RequestUseSkillSlot",4);
    FloUpdateOffset = IL2CPP_Bridge::NewGetMethod(dll,logic,"GameInput","UpdateFrame",0);
    FloResolved = true;
    if (FloUpdateOffset) enqueue(^{
        std::lock_guard<std::recursive_mutex> lock(FloMutex);
        HookUnity(FloUpdateOffset, FloInputUpdate, FloOriginalUpdate);
    });
}
static void FloDiagnostics() {
    std::lock_guard<std::recursive_mutex> lock(FloMutex);
    if (!FloResolved) { ImGui::TextDisabled("Flo: pending initialization"); return; }
    const ImVec4 red(1.f,.45f,.35f,1.f);
    if (!FloActorsReady()) ImGui::TextColored(red,"Missing ActorLinker API / fields");
    if (!FloMove) ImGui::TextColored(red,"Missing: SendMoveDirection(Vector2,Vector2)");
    if (!FloStop) ImGui::TextColored(red,"Missing: GameInput.StopInput");
    if (!FloCanSkill) ImGui::TextColored(red,"Missing: CanRequestSkill");
    if (!FloSkill) ImGui::TextColored(red,"Missing: RequestUseSkillSlot");
    if (!FloUpdateOffset) ImGui::TextColored(red,"Missing: GameInput.UpdateFrame");
    else if (!FloOriginalUpdate) ImGui::TextColored(red,"UpdateFrame hook: pending / failed");
    if (FloReady()) ImGui::TextColored(ImVec4(.4f,.85f,.5f,1.f),"Flo methods: OK");
    if (FloNow() - FloSeen < .5) ImGui::Text("Hero ID: %d%s",FloHeroId,FloHeroId==521 ? " (Florentino)" : "");
    else ImGui::TextDisabled("Waiting for local hero (521)");
    ImGui::Text("Flowers: %zu",FloFlowers.size());
}
static void FloDraw(ImDrawList* draw) {
    std::lock_guard<std::recursive_mutex> lock(FloMutex);
    if (!FloActive() || !HoaPos || !get_main || !WorldToScreens) return;
    const uintptr_t base = getRealOffset(0);
    if ((uintptr_t)get_main == base || (uintptr_t)WorldToScreens == base) return;
    void* cam = get_main(); if (!cam) return;
    for (const auto& f : FloFlowers) {
        if (FloNow() - f.second.time > .5) continue;
        Vector3 sc = WorldToScreens(cam,f.second.pos,2);
        if (sc.z <= 0 || sc.x < 0 || sc.x > 1 || sc.y < 0 || sc.y > 1) continue;
        ImVec2 xy(sc.x*kWidth,(1.f-sc.y)*kHeight);
        draw->AddCircle(xy,5.f,IM_COL32(0,191,255,220),16,1.5f);
        draw->AddCircleFilled(xy,3.f,IM_COL32(255,0,0,230),16);
    }
}

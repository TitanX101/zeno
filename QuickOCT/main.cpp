#include <zen/zen.h>
#include <zen/PrimitiveObject.h>
#include <algorithm>
#include <numeric>
#include <cassert>
#include <stack>

using namespace zenbase;


struct FishYields : zen::INode {
  virtual void apply() override {
    auto stars = get_input("stars")->as<PrimitiveObject>();
    auto rate = std::get<int>(get_param("rate"));

    for (auto &[_, arr]: stars->m_attrs) {
        std::visit([rate](auto &arr) {
            for (int i = 0, j = 0; j < arr.size(); i++, j += rate) {
                arr[i] = arr[j];
            }
        }, arr);
    }
    size_t new_size = stars->size() / rate;
    printf("fish yields new_size = %zd\n", new_size);
    stars->resize(new_size);

    set_output_ref("stars", get_input_ref("stars"));
  }
};

static int defFishYields = zen::defNodeClass<FishYields>("FishYields",
    { /* inputs: */ {
    "stars",
    }, /* outputs: */ {
    "stars",
    }, /* params: */ {
    {"int", "rate", "1 1"},
    }, /* category: */ {
    "NBodySolver",
    }});


struct AdvectStars : zen::INode {
  virtual void apply() override {
    auto stars = get_input("stars")->as<PrimitiveObject>();
    auto &mass = stars->attr<float>("mass");
    auto &pos = stars->attr<zen::vec3f>("pos");
    auto &vel = stars->attr<zen::vec3f>("vel");
    auto &acc = stars->attr<zen::vec3f>("acc");
    auto dt = std::get<float>(get_param("dt"));
    #pragma omp parallel for
    for (int i = 0; i < stars->size(); i++) {
        pos[i] += vel[i] * dt + acc[i] * (dt * dt / 2);
        vel[i] += acc[i] * dt;
    }

    set_output_ref("stars", get_input_ref("stars"));
  }
};

static int defAdvectStars = zen::defNodeClass<AdvectStars>("AdvectStars",
    { /* inputs: */ {
    "stars",
    }, /* outputs: */ {
    "stars",
    }, /* params: */ {
    {"float", "dt", "0.01 0"},
    }, /* category: */ {
    "NBodySolver",
    }});


static int morton3d(zen::vec3f const &pos) {
    zen::vec3i v = zen::clamp(zen::vec3i(zen::floor(pos * 1024)), 0, 1023);

    v = (v | (v << 16)) & 0x030000FF;
    v = (v | (v <<  8)) & 0x0300F00F;
    v = (v | (v <<  4)) & 0x030C30C3;
    v = (v | (v <<  2)) & 0x09249249;

    return (v[0] << 2) | (v[1] << 1) | v[0];
}

struct MortonSorting : zen::INode {
  virtual void apply() override {
    auto stars = get_input("stars")->as<PrimitiveObject>();
    auto &pos = stars->attr<zen::vec3f>("pos");
    std::vector<int> mc(stars->size());

    #pragma omp parallel for
    for (int i = 0; i < stars->size(); i++) {
        mc[i] = morton3d(pos[i]);
    }

    std::vector<int> indices(mc.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&mc](int pos1, int pos2) {
        return mc[pos1] < mc[pos2];
    });

    for (auto &[_, arr]: stars->m_attrs) {
        std::visit([&indices](auto &arr) {
            auto tmparr = arr;  // deep-copy
            for (int i = 0; i < arr.size(); i++) {
                arr[i] = tmparr[indices[i]];
            }
        }, arr);
    }

    set_output_ref("stars", get_input_ref("stars"));
  }
};

static int defMortonSorting = zen::defNodeClass<MortonSorting>("MortonSorting",
    { /* inputs: */ {
    "stars",
    }, /* outputs: */ {
    "stars",
    }, /* params: */ {
    }, /* category: */ {
    "NBodySolver",
    }});


struct LinearOctree : zen::INode {
  virtual void apply() override {
    auto stars = get_input("stars")->as<PrimitiveObject>();
    auto &pos = stars->attr<zen::vec3f>("pos");
    std::vector<int> mc(stars->size());

    #pragma omp parallel for
    for (int i = 0; i < stars->size(); i++) {
        mc[i] = morton3d(pos[i]);
    }

    std::vector<std::array<int, 8>> children(1);

    std::stack<int> stack;
    for (int i = 0; i < stars->size(); i++) {
        stack.push(i);
    }

    while (!stack.empty()) {
        int pid = stack.top(); stack.pop();
        int curr = 0;
        for (int k = 27; k >= 0; k -= 3) {
            int sel = (mc[pid] >> k) & 7;
            int ch = children[curr][sel];
            if (ch == 0) {  // empty
                // directly insert a leaf node
                children[curr][sel] = -pid;
                break;
            } else if (ch > 0) {  // child node
                // then visit into this node
                curr = ch;
            } else {  // leaf node
                // pop the leaf, replace with a child node, and visit later
                stack.push(-ch);
                curr = children[curr][sel] = children.size();
                children.emplace_back();
            }
        }
        assert(k >= 0);
    }

    printf("LinearOctree: %d stars -> %zd nodes\n", stars->size(), children.size());

    set_output_ref("stars", get_input_ref("stars"));
  }
};

static int defLinearOctree = zen::defNodeClass<LinearOctree>("LinearOctree",
    { /* inputs: */ {
    "stars",
    }, /* outputs: */ {
    "stars",
    }, /* params: */ {
    }, /* category: */ {
    "NBodySolver",
    }});


static zen::vec3f gfunc(zen::vec3f const &rij) {
    const float eps = 1e-3;
    float r = eps * eps + zen::dot(rij, rij);
    return rij / (r * zen::sqrt(r));
}

struct ComputeGravity : zen::INode {
  virtual void apply() override {
    auto stars = get_input("stars")->as<PrimitiveObject>();
    auto &mass = stars->attr<float>("mass");
    auto &pos = stars->attr<zen::vec3f>("pos");
    auto &vel = stars->attr<zen::vec3f>("vel");
    auto &acc = stars->attr<zen::vec3f>("acc");
    auto G = std::get<float>(get_param("G"));
    auto eps = std::get<float>(get_param("eps"));
    printf("computing gravity...\n");
    for (int i = 0; i < stars->size(); i++) {
        acc[i] = zen::vec3f(0);
    }
    #pragma omp parallel for
    for (int i = 0; i < stars->size(); i++) {
        for (int j = i + 1; j < stars->size(); j++) {
            auto rij = pos[j] - pos[i];
            float r = eps * eps + zen::dot(rij, rij);
            rij /= r * zen::sqrt(r);
            acc[i] += mass[j] * rij;
            acc[j] -= mass[i] * rij;
        }
    }
    printf("computing gravity done\n");
    for (int i = 0; i < stars->size(); i++) {
        acc[i] *= G;
    }

    set_output_ref("stars", get_input_ref("stars"));
  }
};

static int defComputeGravity = zen::defNodeClass<ComputeGravity>("ComputeGravity",
    { /* inputs: */ {
    "stars",
    }, /* outputs: */ {
    "stars",
    }, /* params: */ {
    {"float", "G", "1.0 0"},
    {"float", "eps", "0.0001 0"},
    }, /* category: */ {
    "NBodySolver",
    }});

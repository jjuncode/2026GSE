#pragma once

#include <array>
#include <vector>

constexpr int32 BT_SUCCESS = 0;
constexpr int32 BT_FAILURE = 1;
constexpr int32 BT_RUNNING = 2;

constexpr int32 BT_NODE_ACTION = 0;
constexpr int32 BT_NODE_CONDITION = 1;
constexpr int32 BT_NODE_SEQUENCE = 2;	// 차례로. 하나라도 실패하면 실패
constexpr int32 BT_NODE_SELECTOR = 3;	// 앞 자식이 우선. 매 틱 처음부터 다시 본다
constexpr int32 BT_NODE_WEIGHTED = 4;	// 가중치 비례로 하나를 뽑아 끝날 때까지 붙든다

constexpr size_t BT_MAX_WEIGHTED_CHILDREN = 16;

// 에이전트 하나가 트리 위에서 쓰는 기억. 트리는 여럿이 같이 쓰고 기억만 각자 든다
struct BtMemory
{
	std::vector<int32> running_child;	// 합성 노드마다 붙들고 있는 자식 순번. -1 = 없음
	std::vector<uint8> leaf_running;	// 잎 노드마다 지난 틱에 진행 중이었나
	std::vector<int32> last_pick;	// 가중 노드마다 마지막으로 뽑은 자식 순번
	std::vector<float> last_pick_chance;	// 그때 그 자식이 뽑힐 확률
	std::vector<int32> active_path;	// 이번 틱에 실패하지 않고 지나간 노드. 관찰창용
	uint32 random_state = 1;
	int32 decision_count = 0;	// 가중 선택을 굴린 횟수
};

// 노드는 배열 하나에 평평하게 든다. 자식을 부모보다 먼저 만들기 때문에
// 한 노드의 서브트리는 언제나 [first, 자기 번호] 연속 구간이다. 그래서 내려놓기가 구간 초기화 한 번이다
template <typename Agent>
class BehaviorTree
{
public:
	typedef int32 (*ActionFunc)(Agent* agent, bool is_enter, float delta_seconds);
	typedef bool (*ConditionFunc)(const Agent& agent);
	typedef float (*WeightFunc)(const Agent& agent);

	int32 AddAction(const wchar_t* name, ActionFunc action)
	{
		Node node;
		node.kind = BT_NODE_ACTION;
		node.name = name;
		node.action = action;

		return AddNode(node);
	}

	int32 AddCondition(const wchar_t* name, ConditionFunc condition)
	{
		Node node;
		node.kind = BT_NODE_CONDITION;
		node.name = name;
		node.condition = condition;

		return AddNode(node);
	}

	int32 AddSequence(const wchar_t* name, const std::vector<int32>& children)
	{
		Node node;
		node.kind = BT_NODE_SEQUENCE;
		node.name = name;
		node.children = children;

		return AddNode(node);
	}

	int32 AddSelector(const wchar_t* name, const std::vector<int32>& children)
	{
		Node node;
		node.kind = BT_NODE_SELECTOR;
		node.name = name;
		node.children = children;

		return AddNode(node);
	}

	int32 AddWeighted(const wchar_t* name, const std::vector<int32>& children, const std::vector<WeightFunc>& weights)
	{
		Node node;
		node.kind = BT_NODE_WEIGHTED;
		node.name = name;
		node.children = children;
		node.weights = weights;

		return AddNode(node);
	}

	void SetRoot(int32 node)
	{
		m_root = node;
	}

	void InitMemory(BtMemory* memory, uint32 seed) const
	{
		const size_t count = m_nodes.size();

		memory->running_child.assign(count, -1);
		memory->leaf_running.assign(count, 0);
		memory->last_pick.assign(count, -1);
		memory->last_pick_chance.assign(count, 0.f);
		memory->active_path.clear();
		memory->decision_count = 0;

		// 0 이면 xorshift 가 영원히 0 이다
		memory->random_state = seed;
		if (memory->random_state == 0)
		{
			memory->random_state = 0x9e3779b9u;
		}
	}

	int32 Tick(Agent* agent, BtMemory* memory, float delta_seconds) const
	{
		memory->active_path.clear();

		if (m_root < 0)
		{
			return BT_FAILURE;
		}

		return TickNode(m_root, agent, memory, delta_seconds);
	}

	// 죽거나 되살아날 때 붙들고 있던 것을 전부 놓는다
	void Reset(BtMemory* memory) const
	{
		if (m_root >= 0)
		{
			ResetSubtree(m_root, memory);
		}

		memory->active_path.clear();
	}

	int32 GetNodeCount() const
	{
		return static_cast<int32>(m_nodes.size());
	}

	const wchar_t* GetNodeName(int32 node) const
	{
		return m_nodes[static_cast<size_t>(node)].name;
	}

	int32 GetNodeKind(int32 node) const
	{
		return m_nodes[static_cast<size_t>(node)].kind;
	}

	const std::vector<int32>& GetChildren(int32 node) const
	{
		return m_nodes[static_cast<size_t>(node)].children;
	}

	float EvaluateWeight(int32 node, size_t order, const Agent& agent) const
	{
		const Node& target = m_nodes[static_cast<size_t>(node)];

		if (order >= target.weights.size())
		{
			return 0.f;
		}

		return ClampWeight(target.weights[order](agent));
	}

	static float NextUnit(BtMemory* memory)
	{
		uint32 value = memory->random_state;
		value ^= value << 13;
		value ^= value >> 17;
		value ^= value << 5;
		memory->random_state = value;

		return static_cast<float>(value >> 8) / 16777216.f;
	}

private:
	struct Node
	{
		int32 kind = BT_NODE_ACTION;
		const wchar_t* name = L"";
		ActionFunc action = nullptr;
		ConditionFunc condition = nullptr;
		std::vector<int32> children;
		std::vector<WeightFunc> weights;
		int32 first = 0;
	};

	static float ClampWeight(float weight)
	{
		if (weight < 0.f)
		{
			return 0.f;
		}

		return weight;
	}

	int32 AddNode(Node node)
	{
		const int32 index = static_cast<int32>(m_nodes.size());

		node.first = index;
		for (int32 child : node.children)
		{
			const int32 child_first = m_nodes[static_cast<size_t>(child)].first;
			if (child_first < node.first)
			{
				node.first = child_first;
			}
		}

		m_nodes.push_back(node);

		return index;
	}

	void ResetSubtree(int32 node, BtMemory* memory) const
	{
		const int32 first = m_nodes[static_cast<size_t>(node)].first;

		for (int32 index = first; index <= node; ++index)
		{
			memory->running_child[static_cast<size_t>(index)] = -1;
			memory->leaf_running[static_cast<size_t>(index)] = 0;
		}
	}

	int32 TickNode(int32 node, Agent* agent, BtMemory* memory, float delta_seconds) const
	{
		const size_t path_mark = memory->active_path.size();
		memory->active_path.push_back(node);

		int32 result = BT_FAILURE;

		switch (m_nodes[static_cast<size_t>(node)].kind)
		{
			case BT_NODE_ACTION:
			{
				result = TickAction(node, agent, memory, delta_seconds);
				break;
			}
			case BT_NODE_CONDITION:
			{
				result = TickCondition(node, *agent);
				break;
			}
			case BT_NODE_SEQUENCE:
			{
				result = TickSequence(node, agent, memory, delta_seconds);
				break;
			}
			case BT_NODE_SELECTOR:
			{
				result = TickSelector(node, agent, memory, delta_seconds);
				break;
			}
			case BT_NODE_WEIGHTED:
			{
				result = TickWeighted(node, agent, memory, delta_seconds);
				break;
			}
			default:
			{
				break;
			}
		}

		// 실패한 가지는 경로에서 뺀다. 관찰창에는 지금 실제로 돌고 있는 줄기만 남는다
		if (result == BT_FAILURE)
		{
			memory->active_path.resize(path_mark);
		}

		return result;
	}

	// 잎은 처음 들어온 틱을 알아야 목표를 새로 잡는다
	int32 TickAction(int32 node, Agent* agent, BtMemory* memory, float delta_seconds) const
	{
		const size_t index = static_cast<size_t>(node);
		const bool is_enter = memory->leaf_running[index] == 0;
		const int32 result = m_nodes[index].action(agent, is_enter, delta_seconds);

		if (result == BT_RUNNING)
		{
			memory->leaf_running[index] = 1;
		}
		else
		{
			memory->leaf_running[index] = 0;
		}

		return result;
	}

	int32 TickCondition(int32 node, const Agent& agent) const
	{
		if (m_nodes[static_cast<size_t>(node)].condition(agent))
		{
			return BT_SUCCESS;
		}

		return BT_FAILURE;
	}

	int32 TickSequence(int32 node, Agent* agent, BtMemory* memory, float delta_seconds) const
	{
		const size_t index = static_cast<size_t>(node);
		const std::vector<int32>& children = m_nodes[index].children;

		size_t start = 0;
		if (memory->running_child[index] >= 0)
		{
			start = static_cast<size_t>(memory->running_child[index]);
		}

		for (size_t order = start; order < children.size(); ++order)
		{
			const int32 result = TickNode(children[order], agent, memory, delta_seconds);

			if (result == BT_RUNNING)
			{
				memory->running_child[index] = static_cast<int32>(order);
				return BT_RUNNING;
			}

			if (result == BT_FAILURE)
			{
				memory->running_child[index] = -1;
				return BT_FAILURE;
			}
		}

		memory->running_child[index] = -1;

		return BT_SUCCESS;
	}

	int32 TickSelector(int32 node, Agent* agent, BtMemory* memory, float delta_seconds) const
	{
		const size_t index = static_cast<size_t>(node);
		const std::vector<int32>& children = m_nodes[index].children;
		const int32 previous = memory->running_child[index];

		for (size_t order = 0; order < children.size(); ++order)
		{
			const int32 result = TickNode(children[order], agent, memory, delta_seconds);

			if (result == BT_FAILURE)
			{
				continue;
			}

			// 윗순위가 이기면 붙들고 있던 아랫순위를 내려놓는다. 안 놓으면 나중에 엉뚱한 단계부터 이어 간다
			if (previous >= 0 && previous != static_cast<int32>(order))
			{
				ResetSubtree(children[static_cast<size_t>(previous)], memory);
			}

			if (result == BT_RUNNING)
			{
				memory->running_child[index] = static_cast<int32>(order);
			}
			else
			{
				memory->running_child[index] = -1;
			}

			return result;
		}

		if (previous >= 0)
		{
			ResetSubtree(children[static_cast<size_t>(previous)], memory);
		}

		memory->running_child[index] = -1;

		return BT_FAILURE;
	}

	int32 TickWeighted(int32 node, Agent* agent, BtMemory* memory, float delta_seconds) const
	{
		const size_t index = static_cast<size_t>(node);
		const Node& weighted = m_nodes[index];
		const int32 held = memory->running_child[index];

		// 한 번 고른 것은 끝날 때까지 붙든다. 매 틱 다시 굴리면 두 걸음 가다 돌아서며 떤다
		if (held >= 0)
		{
			const int32 result = TickNode(weighted.children[static_cast<size_t>(held)], agent, memory, delta_seconds);

			if (result != BT_RUNNING)
			{
				memory->running_child[index] = -1;
			}

			return result;
		}

		const size_t child_count = weighted.children.size();

		std::array<float, BT_MAX_WEIGHTED_CHILDREN> weights = {};
		std::array<bool, BT_MAX_WEIGHTED_CHILDREN> excluded = {};

		for (size_t order = 0; order < child_count && order < BT_MAX_WEIGHTED_CHILDREN; ++order)
		{
			weights[order] = EvaluateWeight(node, order, *agent);
		}

		// 뽑은 것이 불가능하면 ( 표적 없음 ) 그것만 빼고 남은 것끼리 다시 굴린다
		for (size_t attempt = 0; attempt < child_count; ++attempt)
		{
			float total = 0.f;
			for (size_t order = 0; order < child_count; ++order)
			{
				if (excluded[order] == false)
				{
					total += weights[order];
				}
			}

			if (total <= 0.f)
			{
				return BT_FAILURE;
			}

			const float roll = NextUnit(memory) * total;
			float cursor = 0.f;
			size_t pick = child_count;

			for (size_t order = 0; order < child_count; ++order)
			{
				if (excluded[order] || weights[order] <= 0.f)
				{
					continue;
				}

				cursor += weights[order];
				pick = order;

				if (roll < cursor)
				{
					break;
				}
			}

			if (pick >= child_count)
			{
				return BT_FAILURE;
			}

			memory->last_pick[index] = static_cast<int32>(pick);
			memory->last_pick_chance[index] = weights[pick] / total;
			memory->decision_count += 1;

			const int32 child = weighted.children[pick];
			const int32 result = TickNode(child, agent, memory, delta_seconds);

			if (result == BT_RUNNING)
			{
				memory->running_child[index] = static_cast<int32>(pick);
				return BT_RUNNING;
			}

			if (result == BT_SUCCESS)
			{
				return BT_SUCCESS;
			}

			ResetSubtree(child, memory);
			excluded[pick] = true;
		}

		return BT_FAILURE;
	}

	std::vector<Node> m_nodes;
	int32 m_root = -1;
};

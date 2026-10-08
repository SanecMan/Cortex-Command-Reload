#pragma once

namespace RTE::ObjectScriptExecution {

// Reconciliation changes native ownership without executing gameplay callbacks.
// Keep this per thread: authoritative simulation on another thread remains enabled.
inline thread_local bool Suppressed = false;

class SuppressionScope {
public:
	SuppressionScope() : m_Previous(Suppressed) { Suppressed = true; }
	~SuppressionScope() { Suppressed = m_Previous; }
	SuppressionScope(const SuppressionScope&) = delete;
	SuppressionScope& operator=(const SuppressionScope&) = delete;

private:
	bool m_Previous;
};

} // namespace RTE::ObjectScriptExecution

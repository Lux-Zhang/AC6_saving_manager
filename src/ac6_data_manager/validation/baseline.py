from __future__ import annotations

from dataclasses import dataclass
from enum import StrEnum
from typing import Iterable


class GateId(StrEnum):
    RECORD_MAP = "record_map_proven"
    DEPENDENCY_GRAPH = "dependency_graph_proven"
    EDITABILITY_TRANSITION = "editability_transition_proven"
    PREVIEW_PROVENANCE = "preview_provenance_proven"
    ROUND_TRIP_CORPUS = "round_trip_corpus_proven"


class MilestoneId(StrEnum):
    M0 = "M0"
    M05 = "M0.5"
    M1 = "M1"


class Capability(StrEnum):
    GOLDEN_CORPUS = "golden_corpus"
    AUDIT = "audit"
    ROLLBACK = "rollback"
    EMBLEM_STABLE_LANE = "emblem_stable_lane"
    AC_READ_ONLY_BASELINE = "ac_read_only_baseline"
    AC_DRY_RUN = "ac_dry_run"
    AC_EXPERIMENTAL_IMPORT = "ac_experimental_import"


@dataclass(frozen=True, slots=True)
class ValidationGateDefinition:
    gate_id: GateId
    display_name: str
    sample_scope: tuple[str, ...]
    evidence_paths: tuple[str, ...]
    pass_conditions: tuple[str, ...]
    failure_conditions: tuple[str, ...]
    unlocks_capabilities: tuple[Capability, ...]


@dataclass(frozen=True, slots=True)
class ValidationCheckDefinition:
    check_id: str
    stage: str
    description: str
    pass_criteria: str
    evidence_hint: str


@dataclass(frozen=True, slots=True)
class MilestoneDefinition:
    milestone_id: MilestoneId
    display_name: str
    checks: tuple[ValidationCheckDefinition, ...]


@dataclass(frozen=True, slots=True)
class CapabilityDecision:
    capability: Capability
    allowed: bool
    reason: str
    blocking_gates: tuple[GateId, ...] = ()


@dataclass(frozen=True, slots=True)
class ValidationBaseline:
    gates: dict[GateId, ValidationGateDefinition]
    milestones: dict[MilestoneId, MilestoneDefinition]
    artifact_roots: tuple[str, ...]

    def required_artifact_paths(self) -> tuple[str, ...]:
        paths: set[str] = set(self.artifact_roots)
        for gate in self.gates.values():
            paths.update(gate.evidence_paths)
        return tuple(sorted(paths))

    def required_checks(
        self,
        milestone_ids: Iterable[MilestoneId] = (MilestoneId.M0, MilestoneId.M05, MilestoneId.M1),
    ) -> tuple[ValidationCheckDefinition, ...]:
        checks: list[ValidationCheckDefinition] = []
        for milestone_id in milestone_ids:
            checks.extend(self.milestones[milestone_id].checks)
        return tuple(checks)

    def assess_capability(
        self,
        capability: Capability,
        proven_gates: Iterable[GateId],
    ) -> CapabilityDecision:
        proven = set(proven_gates)

        if capability in {
            Capability.GOLDEN_CORPUS,
            Capability.AUDIT,
            Capability.ROLLBACK,
            Capability.EMBLEM_STABLE_LANE,
            Capability.AC_READ_ONLY_BASELINE,
        }:
            return CapabilityDecision(
                capability=capability,
                allowed=True,
                reason="Capability is within the M0/M0.5/M1 validation baseline and does not require AC mutation.",
            )

        if capability is Capability.AC_DRY_RUN:
            required = (GateId.RECORD_MAP, GateId.DEPENDENCY_GRAPH)
            blocking = tuple(gate for gate in required if gate not in proven)
            if blocking:
                return CapabilityDecision(
                    capability=capability,
                    allowed=False,
                    reason="AC dry-run requires record-map and dependency-graph proof before it can be enabled.",
                    blocking_gates=blocking,
                )
            return CapabilityDecision(
                capability=capability,
                allowed=True,
                reason="Record-map and dependency-graph proof exist, so AC dry-run may be evaluated under controlled validation.",
            )

        if capability is Capability.AC_EXPERIMENTAL_IMPORT:
            blocking = tuple(gate for gate in GateId if gate not in proven)
            if blocking:
                return CapabilityDecision(
                    capability=capability,
                    allowed=False,
                    reason="PRD requires all five AC gates before experimental import can be opened.",
                    blocking_gates=blocking,
                )
            return CapabilityDecision(
                capability=capability,
                allowed=True,
                reason="All five AC gates are proven; experimental import may be considered by governance review.",
            )

        raise ValueError(f"Unsupported capability: {capability}")


REVERSE_DOC_ROOT = "docs/reverse"
ARTIFACT_ROOT = "artifacts"


def build_default_baseline() -> ValidationBaseline:
    gates = {
        GateId.RECORD_MAP: ValidationGateDefinition(
            gate_id=GateId.RECORD_MAP,
            display_name="record map proven",
            sample_scope=(
                "At least 30 archive samples.",
                "Cover empty, sparse, medium, near-full and multi-share records.",
                "Mix player originals, controlled variants and differential experiments.",
            ),
            evidence_paths=(
                f"{REVERSE_DOC_ROOT}/ac-record-map.md",
                f"{ARTIFACT_ROOT}/ac-record-diff-corpus",
            ),
            pass_conditions=(
                "100% stable record-boundary identification.",
                "Critical-field classification accuracy >= 95%.",
                "Unknown bytes <= 10% without breaking read-only browsing.",
            ),
            failure_conditions=(
                "Any sample cannot locate record boundaries.",
                "Any slot attribution is unstable.",
                "Critical-field classification accuracy < 95%.",
            ),
            unlocks_capabilities=(Capability.AC_READ_ONLY_BASELINE,),
        ),
        GateId.DEPENDENCY_GRAPH: ValidationGateDefinition(
            gate_id=GateId.DEPENDENCY_GRAPH,
            display_name="dependency graph proven",
            sample_scope=(
                "15 high-complexity samples selected from the 30-sample corpus.",
                "Every sample covers emblem, image-decal and preview dependency candidates.",
                "At least 10 delete/replace differential experiments.",
            ),
            evidence_paths=(
                f"{REVERSE_DOC_ROOT}/ac-dependency-graph.md",
                f"{ARTIFACT_ROOT}/ac-dependency-traces",
            ),
            pass_conditions=(
                "Dependency precision >= 95%.",
                "Dependency recall >= 95%.",
                "Delete-impact simulation matches manual validation >= 90%.",
            ),
            failure_conditions=(
                "High-complexity sample contains unexplained critical dependency loss.",
                "Delete-impact error rate exceeds 10%.",
            ),
            unlocks_capabilities=(Capability.AC_DRY_RUN,),
        ),
        GateId.EDITABILITY_TRANSITION: ValidationGateDefinition(
            gate_id=GateId.EDITABILITY_TRANSITION,
            display_name="editability transition proven",
            sample_scope=(
                "At least 12 share->user controlled experiments.",
                "Cover pure AC, heavy decal, emblem-linked and near-full-slot cases.",
            ),
            evidence_paths=(
                f"{REVERSE_DOC_ROOT}/ac-editability-transition.md",
                f"{ARTIFACT_ROOT}/ac-transition-experiments",
            ),
            pass_conditions=(
                "At least 11/12 experiments enter edit mode and save successfully.",
                "Critical dependencies retained >= 95%.",
            ),
            failure_conditions=(
                "Any experiment causes unrecoverable corruption.",
                "Editability success rate < 90%.",
            ),
            unlocks_capabilities=(Capability.AC_EXPERIMENTAL_IMPORT,),
        ),
        GateId.PREVIEW_PROVENANCE: ValidationGateDefinition(
            gate_id=GateId.PREVIEW_PROVENANCE,
            display_name="preview provenance proven",
            sample_scope=(
                "At least 20 AC samples with preview/no-preview/source variations.",
                "At least 8 field-change to preview-change experiments.",
            ),
            evidence_paths=(
                f"{REVERSE_DOC_ROOT}/ac-preview-provenance.md",
                f"{ARTIFACT_ROOT}/ac-preview-traces",
            ),
            pass_conditions=(
                "Preview strategy classification accuracy >= 90%.",
                "Extraction success >= 95% or fallback success >= 90% within 2 seconds average.",
            ),
            failure_conditions=(
                "Preview source remains unattributed.",
                "Preview strategy classification accuracy < 90%.",
            ),
            unlocks_capabilities=(Capability.AC_READ_ONLY_BASELINE,),
        ),
        GateId.ROUND_TRIP_CORPUS: ValidationGateDefinition(
            gate_id=GateId.ROUND_TRIP_CORPUS,
            display_name="round-trip corpus proven",
            sample_scope=(
                "At least 20 emblem write/readback samples.",
                "At least 12 AC experimental write samples.",
                "Cover add, replace, delete rollback, package import/export and interrupted mutation.",
            ),
            evidence_paths=(
                f"{ARTIFACT_ROOT}/roundtrip-reports",
                f"{ARTIFACT_ROOT}/golden-corpus",
            ),
            pass_conditions=(
                "Emblem round-trip success is 100%.",
                "AC experimental round-trip success >= 90% with zero unrecoverable corruption.",
                "Rollback success is 100%.",
            ),
            failure_conditions=(
                "Any unrecoverable corruption.",
                "Any rollback failure.",
                "Emblem round-trip success below 100%.",
            ),
            unlocks_capabilities=(Capability.EMBLEM_STABLE_LANE, Capability.ROLLBACK),
        ),
    }

    milestones = {
        MilestoneId.M0: MilestoneDefinition(
            milestone_id=MilestoneId.M0,
            display_name="Corpus & Reverse-Engineering Baseline",
            checks=(
                ValidationCheckDefinition(
                    check_id="M0-unit-index-tags",
                    stage="unit",
                    description="Sample indexing, tagging and metadata readers behave deterministically.",
                    pass_criteria="Golden corpus metadata reads are reproducible with 100% sample index coverage.",
                    evidence_hint="artifacts/golden-corpus/manifest.example.json",
                ),
                ValidationCheckDefinition(
                    check_id="M0-integration-workspace-scan",
                    stage="integration",
                    description="Unpack -> workspace -> scan completes for every baseline sample.",
                    pass_criteria="Workspace scan success rate is 100% for the M0 corpus.",
                    evidence_hint="artifacts/golden-corpus/README.md",
                ),
                ValidationCheckDefinition(
                    check_id="M0-gui-baseline-browser",
                    stage="gui-e2e",
                    description="Baseline browser renders sample metadata without mutation controls.",
                    pass_criteria="Metadata cards render consistently for all indexed samples.",
                    evidence_hint="docs/appendices/ac6-data-manager.validation-baseline.md",
                ),
                ValidationCheckDefinition(
                    check_id="M0-fault-injection-fail-closed",
                    stage="fault-injection",
                    description="Missing files, missing tools and permission failures must fail closed.",
                    pass_criteria="All injected failures are blocked before mutation and produce audit evidence.",
                    evidence_hint="artifacts/incidents/README.md",
                ),
            ),
        ),
        MilestoneId.M05: MilestoneDefinition(
            milestone_id=MilestoneId.M05,
            display_name="Save Mutation Kernel Hardening",
            checks=(
                ValidationCheckDefinition(
                    check_id="M05-unit-restore-point-manifest",
                    stage="unit",
                    description="Restore-point naming, hashing and manifest generation stay deterministic.",
                    pass_criteria="Restore-point metadata always includes workspace, hash and rule-set identifiers.",
                    evidence_hint="artifacts/audit/README.md",
                ),
                ValidationCheckDefinition(
                    check_id="M05-integration-shadow-readback",
                    stage="integration",
                    description="Shadow workspace re-pack and readback validation reject inconsistent writes.",
                    pass_criteria="Readback mismatch is always blocked and routed into rollback orchestration.",
                    evidence_hint="artifacts/roundtrip-reports/README.md",
                ),
                ValidationCheckDefinition(
                    check_id="M05-gui-dry-run-gate",
                    stage="gui-e2e",
                    description="Dry-run and ImportPlan views cannot enable Apply when validation fails.",
                    pass_criteria="Invalid plans remain blocked with visible audit context and rollback entry points.",
                    evidence_hint="docs/ac6-data-manager.technical-design.md",
                ),
                ValidationCheckDefinition(
                    check_id="M05-fault-injection-rollback",
                    stage="fault-injection",
                    description="Interrupted re-pack, readback failure and tool mismatch all trigger rollback.",
                    pass_criteria="Rollback success rate remains 100% across hardened failure scenarios.",
                    evidence_hint="artifacts/incidents/README.md",
                ),
            ),
        ),
        MilestoneId.M1: MilestoneDefinition(
            milestone_id=MilestoneId.M1,
            display_name="Emblem MVP",
            checks=(
                ValidationCheckDefinition(
                    check_id="M1-unit-emblem-roundtrip",
                    stage="unit",
                    description="Emblem parser, slot allocation and package manifest cooperate with the validation lane.",
                    pass_criteria="Golden corpus hash list and audit trail cover emblem add/replace/import/export paths.",
                    evidence_hint="artifacts/golden-corpus/manifest.example.json",
                ),
                ValidationCheckDefinition(
                    check_id="M1-integration-apply-readback-rollback",
                    stage="integration",
                    description="Manual share-emblem import produces consistent readback or automatic rollback.",
                    pass_criteria="Every write path yields readback evidence; failures create incident bundles without mutating the original save.",
                    evidence_hint="artifacts/roundtrip-reports/README.md",
                ),
                ValidationCheckDefinition(
                    check_id="M1-gui-audit-view",
                    stage="gui-e2e",
                    description="GUI exposes audit view, rollback entry point and risk annotations.",
                    pass_criteria="Audit view shows apply, rollback and blocked actions with complete metadata.",
                    evidence_hint="artifacts/audit/README.md",
                ),
                ValidationCheckDefinition(
                    check_id="M1-fault-injection-slot-conflict",
                    stage="fault-injection",
                    description="Slot-full, conflict, package-checksum and readback failures stay recoverable.",
                    pass_criteria="All conflict scenarios fail closed and preserve incident artifacts for triage.",
                    evidence_hint="artifacts/incidents/README.md",
                ),
            ),
        ),
    }

    return ValidationBaseline(
        gates=gates,
        milestones=milestones,
        artifact_roots=(
            f"{ARTIFACT_ROOT}/golden-corpus",
            f"{ARTIFACT_ROOT}/audit",
            f"{ARTIFACT_ROOT}/incidents",
            f"{ARTIFACT_ROOT}/roundtrip-reports",
        ),
    )

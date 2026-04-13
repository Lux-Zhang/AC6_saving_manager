from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Mapping, Protocol, Sequence

UNPACK_METADATA_FILENAME = ".ac6dm_unpack_meta.json"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


@dataclass(frozen=True)
class ToolchainInfo:
    name: str
    executable: Path
    executable_sha256: str
    version: str | None
    launcher: tuple[str, ...] = ()
    required_version: str | None = None
    required_sha256: str | None = None


@dataclass(frozen=True)
class CommandReport:
    command: tuple[str, ...]
    returncode: int
    stdout: str
    stderr: str


class ToolchainError(RuntimeError):
    """Raised when the configured external toolchain is unavailable or mismatched."""


class ToolExecutionError(RuntimeError):
    """Raised when the external toolchain returns a failure."""


class ContainerAdapter(Protocol):
    def describe_toolchain(self) -> ToolchainInfo:
        """Return a stable descriptor for audit and environment checks."""

    def unpack(self, container_path: Path, *, expected_directory: Path | None = None) -> Path:
        """Unpack a container file into a directory and return that directory."""

    def repack(self, unpacked_directory: Path, *, expected_container: Path | None = None) -> Path:
        """Repack an unpacked directory into a container file and return that file."""


class WitchyBndAdapter:
    def __init__(
        self,
        executable: Path,
        *,
        launcher: Sequence[str] | None = None,
        env: Mapping[str, str] | None = None,
        timeout_seconds: int = 300,
        required_version: str | None = None,
        required_sha256: str | None = None,
    ) -> None:
        self.executable = Path(executable)
        self.launcher = tuple(launcher or ())
        self.env = dict(env or {})
        self.timeout_seconds = timeout_seconds
        self.required_version = required_version
        self.required_sha256 = required_sha256

    def describe_toolchain(self) -> ToolchainInfo:
        if not self.executable.exists():
            raise ToolchainError(f"WitchyBND executable does not exist: {self.executable}")
        if not self.executable.is_file():
            raise ToolchainError(f"WitchyBND executable is not a file: {self.executable}")

        executable_sha256 = sha256_file(self.executable)
        if self.required_sha256 and executable_sha256 != self.required_sha256:
            raise ToolchainError(
                "WitchyBND executable hash mismatch: "
                f"expected {self.required_sha256}, got {executable_sha256}"
            )

        version = self._probe_version()
        if self.required_version and version != self.required_version:
            raise ToolchainError(
                "WitchyBND version mismatch: "
                f"expected {self.required_version}, got {version or 'unknown'}"
            )

        return ToolchainInfo(
            name="WitchyBND",
            executable=self.executable,
            executable_sha256=executable_sha256,
            version=version,
            launcher=self.launcher,
            required_version=self.required_version,
            required_sha256=self.required_sha256,
        )

    def unpack(self, container_path: Path, *, expected_directory: Path | None = None) -> Path:
        container_path = Path(container_path)
        if not container_path.exists():
            raise ToolchainError(f"Container file does not exist: {container_path}")
        if not container_path.is_file():
            raise ToolchainError(f"Container path is not a file: {container_path}")

        self._run(container_path)

        actual_directory = self._default_unpack_directory(container_path)
        if not actual_directory.exists():
            raise ToolExecutionError(
                f"WitchyBND did not create the expected unpacked directory: {actual_directory}"
            )

        unpacked_directory = self._relocate_output(actual_directory, expected_directory)
        self._write_unpack_metadata(unpacked_directory, container_path)
        return unpacked_directory

    def repack(self, unpacked_directory: Path, *, expected_container: Path | None = None) -> Path:
        unpacked_directory = Path(unpacked_directory)
        if not unpacked_directory.exists():
            raise ToolchainError(f"Unpacked directory does not exist: {unpacked_directory}")
        if not unpacked_directory.is_dir():
            raise ToolchainError(f"Unpacked path is not a directory: {unpacked_directory}")

        metadata = self._read_unpack_metadata(unpacked_directory)
        self._run(unpacked_directory)

        actual_container = unpacked_directory.parent / metadata["container_name"]
        if not actual_container.exists():
            raise ToolExecutionError(
                f"WitchyBND did not create the expected repacked container: {actual_container}"
            )

        backup_path = actual_container.with_suffix(actual_container.suffix + ".bak")
        if backup_path.exists():
            backup_path.unlink()

        return self._relocate_output(actual_container, expected_container)

    def _probe_version(self) -> str | None:
        try:
            report = self._run("--version", check=False)
        except (ToolExecutionError, ToolchainError):
            return None

        if report.returncode != 0:
            return None

        version = report.stdout.strip() or report.stderr.strip()
        return version or None

    def _run(self, target: str | Path, *, check: bool = True) -> CommandReport:
        command = [*self.launcher, str(self.executable), str(target)]
        env = os.environ.copy()
        env.update(self.env)
        completed = subprocess.run(
            command,
            check=False,
            capture_output=True,
            text=True,
            timeout=self.timeout_seconds,
            env=env,
        )
        report = CommandReport(
            command=tuple(command),
            returncode=completed.returncode,
            stdout=completed.stdout,
            stderr=completed.stderr,
        )
        if check and completed.returncode != 0:
            raise ToolExecutionError(
                "WitchyBND invocation failed: "
                f"command={report.command} returncode={report.returncode} stderr={report.stderr.strip()}"
            )
        return report

    @staticmethod
    def _default_unpack_directory(container_path: Path) -> Path:
        suffix = container_path.suffix.lstrip(".")
        if suffix:
            unpack_name = f"{container_path.stem}-{suffix}"
        else:
            unpack_name = f"{container_path.name}-unpacked"
        return container_path.with_name(unpack_name)

    @staticmethod
    def _relocate_output(actual_path: Path, requested_path: Path | None) -> Path:
        if requested_path is None:
            return actual_path

        requested_path = Path(requested_path)
        if actual_path.resolve() == requested_path.resolve():
            return actual_path

        requested_path.parent.mkdir(parents=True, exist_ok=True)
        if requested_path.exists():
            if requested_path.is_dir():
                shutil.rmtree(requested_path)
            else:
                requested_path.unlink()
        shutil.move(str(actual_path), str(requested_path))
        return requested_path

    @staticmethod
    def _write_unpack_metadata(unpacked_directory: Path, container_path: Path) -> None:
        payload = {
            "container_name": container_path.name,
            "container_path": str(container_path),
        }
        (unpacked_directory / UNPACK_METADATA_FILENAME).write_text(
            json.dumps(payload, ensure_ascii=True, indent=2) + "\n",
            encoding="utf-8",
        )

    @staticmethod
    def _read_unpack_metadata(unpacked_directory: Path) -> dict[str, str]:
        metadata_path = unpacked_directory / UNPACK_METADATA_FILENAME
        if not metadata_path.exists():
            raise ToolchainError(
                "Unpacked directory is missing container metadata: "
                f"{metadata_path}"
            )
        payload = json.loads(metadata_path.read_text(encoding="utf-8"))
        container_name = payload.get("container_name")
        if not isinstance(container_name, str) or not container_name:
            raise ToolchainError(
                "Unpacked directory metadata is invalid: "
                f"{metadata_path}"
            )
        return {"container_name": container_name}

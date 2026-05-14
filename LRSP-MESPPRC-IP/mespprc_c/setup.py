from __future__ import annotations

from pathlib import Path

from setuptools import Extension, setup


ROOT = Path(__file__).resolve().parent
MODULE_NAMES = [
    "instance",
    "label",
    "route",
    "phase1",
    "phase2_dp",
    "phase2_ip",
    "instance_generator",
]


extensions = [
    Extension(
        name=f"mespprc_c.{module_name}",
        sources=[str(ROOT / f"{module_name}.c")],
    )
    for module_name in MODULE_NAMES
]


setup(
    name="mespprc_c",
    version="0.1.0",
    description="C-translated extension build for the MESPPRC research prototype",
    package_dir={"mespprc_c": "."},
    packages=["mespprc_c"],
    ext_modules=extensions,
    include_package_data=True,
    install_requires=[
        "pulp>=3.3",
    ],
    zip_safe=False,
)

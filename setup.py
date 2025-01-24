from setuptools import setup, find_packages

setup(
    name="py_unreal",
    use_scm_version=True,
    setup_requires=["setuptools_scm"],
    package_dir={
        "": "src"
    },
    packages=find_packages(where="src"),
    install_requires=[
        "unreal_core==0.0.1"
    ],
    python_requires=">=3.6",
    package_data={
        "": ["*"]
    },
    include_package_data=True,
)

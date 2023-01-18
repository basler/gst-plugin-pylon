import setuptools

setuptools.setup(
    name="pygstpylon",
    version="0.6.0",
    author="Basler AG",
    description="python bindings to gstpylon",
    url="baslerweb.com",
    packages=[''],
    package_data={'': ['pygstpylon.so']},
    install_requires=["pgi", "PyGObject"]
)

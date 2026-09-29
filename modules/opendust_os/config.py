def can_build(env, platform):
    # Bridge tools are optional (guarded by MODULE_OPENDUST_AGENT_ENABLED); we
    # declare the dependency so build order and module init order are stable.
    env.module_add_dependencies("opendust_os", ["opendust_agent"], True)
    return True


def configure(env):
    pass


def get_doc_classes():
    return [
        "Device",
        "Device2D",
        "OSApp",
        "OSAppContext",
        "OSKernel",
        "OSShell",
        "OpenDustIdentity",
        "WorldDrive",
    ]


def get_doc_path():
    return "doc_classes"

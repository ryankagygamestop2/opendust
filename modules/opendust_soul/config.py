def can_build(env, platform):
    # Soft dependency: the agent.* bridge tools only register when opendust_agent is present.
    env.module_add_dependencies("opendust_soul", ["opendust_agent"], True)
    return not env["disable_3d"]


def configure(env):
    pass


def get_doc_classes():
    return [
        "AgentBody",
        "Soul",
    ]


def get_doc_path():
    return "doc_classes"

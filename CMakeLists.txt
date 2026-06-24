idf_component_register(SRCS "src/my_component.c"
                       INCLUDE_DIRS "include"
                       REQUIRES  # 该组件依赖的其他组件（可选）
                       # PRIV_REQUIRES  # 私有依赖
                       )
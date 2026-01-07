export const SETTINGS_SOURCE = {
  GROUP: "group",
  HERE: "here",
  CLIENT: "client",
} as const;

export const USE_VALUES = {
  [SETTINGS_SOURCE.GROUP]: Math.pow(2, 0),
  [SETTINGS_SOURCE.HERE]: Math.pow(2, 1),
  [SETTINGS_SOURCE.CLIENT]: Math.pow(2, 2),
} as const;

export interface EnabledState {
  name: string;
  enabled: 0 | 1;
  use: (typeof USE_VALUES)[keyof typeof USE_VALUES];
}

export function getUse(enabledStates: EnabledState[], use = 0) {
  const [current] = enabledStates;

  if (!current) {
    return use;
  }

  const newUse = current.enabled ? (use |= current.use) : use;

  return getUse(enabledStates.slice(1), newUse);
}

export function getEnabledState(use: number): EnabledState[] {
  const state = Object.entries(USE_VALUES).map(([k, v]) => ({
    name: k,
    enabled: use & v ? 1 : (0 as EnabledState["enabled"]),
    use: v,
  }));

  return state;
}

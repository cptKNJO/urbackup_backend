import { useRef, useState } from "react";
import { ZodMiniObject } from "zod/v4-mini";
import {
  Body1,
  Dropdown,
  DropdownProps,
  Label,
  Option,
  Button,
  Field,
  Input,
  InputProps,
  Subtitle2,
} from "@fluentui/react-components";

import type {
  ClientSettingState,
  SettingState,
} from "../../../../api/urbackupserver";
import { CheckboxFieldUncontrolled } from "../../Form/CheckboxField";

import styles from "../../Form/TextField.module.css";
import { ArrowResetRegular } from "@fluentui/react-icons";
import {
  type EnabledState,
  getEnabledState,
  getUse,
  SETTINGS_SOURCE,
} from "./utils";

export function MergeableFields({
  name,
  label,
  initialFormState,
}: {
  name: string;
  lable: string;
  initialFormState: ClientSettingState;
}) {
  const nameUse = `${name}.use`;

  const [formData, setFormData] = useState(() => {
    return initialFormState;
  });

  const [hereValue, setHereValue] = useState(initialFormState.value);

  const [enabledStates, setEnabledStates] = useState<EnabledState[]>(() =>
    getEnabledState(initialFormState.use ?? 0),
  );

  const getHereValue = () => {
    const isHereValueEnabled = isSourceEnabled(
      enabledStates,
      SETTINGS_SOURCE.HERE,
    );

    // Send no changes to "HERE" value if the selected source is not HERE.
    // Even if the field was interacted with.
    if (!isHereValueEnabled) {
      return initialFormState.value;
    }

    return hereValue;
  };

  const updateUse = (
    source: (typeof SETTINGS_SOURCE)[keyof typeof SETTINGS_SOURCE],
    value: boolean,
  ) => {
    setEnabledStates((p) => {
      return p.reduce((all, curr) => {
        if (curr.name !== source) {
          return [...all, curr];
        }

        const updatedSource: EnabledState = {
          ...curr,
          enabled: +value as 0 | 1,
        };

        return [...all, updatedSource];
      }, [] as EnabledState[]);
    });
  };

  const hiddenInputValues = {
    value: getHereValue(),
    use: getUse(enabledStates),
  };

  const inputs = [
    {
      name: SETTINGS_SOURCE.HERE,
      labels: makeLabels(SETTINGS_SOURCE.HERE, label),
      state: isSourceEnabled(enabledStates, SETTINGS_SOURCE.HERE),
      value: hiddenInputValues.value,
    },
    {
      name: SETTINGS_SOURCE.GROUP,
      labels: makeLabels(SETTINGS_SOURCE.GROUP, label),
      readonly: true,
      state: isSourceEnabled(enabledStates, SETTINGS_SOURCE.GROUP),
      value: formData.value_group,
    },
    {
      name: SETTINGS_SOURCE.CLIENT,
      labels: makeLabels(SETTINGS_SOURCE.CLIENT, label),
      readonly: true,
      state: isSourceEnabled(enabledStates, SETTINGS_SOURCE.CLIENT),
      value: formData.value_client,
    },
  ] as const;

  return (
    <>
      <input type="hidden" name={name} value={hiddenInputValues.value} />
      <input type="hidden" name={nameUse} value={hiddenInputValues.use} />
      <div
        className="flow flow-s"
        style={{
          paddingBlock: 0,
        }}
      >
        {inputs.map((input) => (
          <CheckboxTextField
            key={input.name}
            checked={input.state}
            value={input.value}
            checkboxLabel={input.labels.checkbox}
            textLabel={input.labels.input}
            inputProps={{
              readOnly: input?.readonly,
              onChange: (_, { value }) => {
                setHereValue(value);
              },
            }}
            onCheckboxChange={(checked) => {
              updateUse(input.name, checked);
            }}
          />
        ))}
      </div>
    </>
  );
}

function MergeableTextField({
  label,
  hint,
  name,
  defaultValue,
  type,
  validationMessage,
  inputProps,
}: {
  label: string;
  description: React.ReactNode;
  hint?: string;
  name: string;
  defaultValue: string;
  type: InputProps["type"];
  inputProps?: InputProps;
  validationMessage?: string;
  children?: React.ReactNode;
}) {
  const initialValue = useRef(defaultValue);

  const [_value, setValue] = useState(defaultValue);

  const showHint = !validationMessage;

  const inputValue = inputProps?.value ?? _value;

  const hasValueChanged = _value !== initialValue.current;

  const resetToInitialValue = () => {
    {
      const newValue = initialValue.current;

      setValue(newValue);
      inputProps?.onChange?.(null, { value: newValue });
    }
  };

  const labelValue = {
    children: label,
    className: "visually-hidden",
  };

  return (
    <Field
      hint={showHint ? hint : undefined}
      label={labelValue}
      {...(validationMessage && {
        validationState: "error",
        validationMessage,
      })}
      orientation="vertical"
      style={{
        display: "inline",
      }}
    >
      <div className={styles["reset-input"]}>
        <Button
          appearance="subtle"
          icon={<ArrowResetRegular />}
          data-hidden={inputProps?.readOnly || !hasValueChanged}
          onClick={resetToInitialValue}
        >
          Reset
        </Button>
        <Input
          name={name}
          type={type}
          {...inputProps}
          value={inputValue}
          onChange={(e, data) => {
            const { value } = data;
            setValue(value);
            inputProps?.onChange?.(e, data);
          }}
        />
      </div>
    </Field>
  );
}

function CheckboxTextField({
  checked,
  value,
  checkboxLabel,
  onCheckboxChange,
  textLabel,
  inputProps,
}: {
  checked: boolean;
  value: string;
  checkboxLabel: string;
  textLabel: string;
}) {
  const [enabled, setEnabled] = useState(checked);

  return (
    <div className="repel">
      <CheckboxFieldUncontrolled
        style={{}}
        label={checkboxLabel}
        checked={enabled}
        onChange={(_, d) => {
          setEnabled(d.checked as boolean);
          onCheckboxChange(d.checked);
        }}
      />
      {/* TODO: should be disabled but is that good for accessibility */}
      <div
        style={{
          display: enabled ? "inline" : "none",
        }}
      >
        <MergeableTextField
          label={textLabel}
          defaultValue={value}
          type="text"
          inputProps={{
            ...inputProps,
          }}
        />
      </div>
    </div>
  );
}

function isSourceEnabled(
  enabledStates: EnabledState[],
  source: (typeof SETTINGS_SOURCE)[keyof typeof SETTINGS_SOURCE],
) {
  const state = enabledStates.find((es) => es.name === source);

  return Boolean(state!.enabled);
}

function makeLabels(
  source: (typeof SETTINGS_SOURCE)[keyof typeof SETTINGS_SOURCE],
  label: string,
) {
  return {
    checkbox: `Apply setting from ${source} for ${label}`,
    input: `Settings for ${label} from ${source}`,
  };
}

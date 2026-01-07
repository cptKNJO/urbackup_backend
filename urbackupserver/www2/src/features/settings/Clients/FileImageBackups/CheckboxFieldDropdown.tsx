import { useRef, useState } from "react";
import {
  Body1,
  Button,
  Dropdown,
  DropdownProps,
  Field as FUIField,
  Input,
  InputProps,
  Label,
  Option,
} from "@fluentui/react-components";
import { ArrowResetRegular } from "@fluentui/react-icons";

import type { ClientSettingState } from "../../../../api/urbackupserver";
import { CheckboxFieldUncontrolled } from "../../Form/CheckboxField";
import { type Field } from "../../Form/types";
import styles from "../../Form/TextField.module.css";

interface InitialFormState {
  use: 1 | 2 | 4;
  value_group: number;
  value?: number;
  value_client?: number;
}

export const VALUE_TO_USE: Record<
  number,
  Exclude<keyof ClientSettingState, "use">
> = {
  1: "value_group",
  2: "value",
  4: "value_client",
};

const HERE_SETTINGS = 2;

const FORMATTED_VALUE_TO_USE = {
  value_group: {
    name: "Group",
    desc: "Use setting from group",
  },
  value: {
    name: "Here",
    desc: "Use setting configured here",
  },
  value_client: {
    name: "Client",
    desc: "Use setting configured on client",
  },
};

function selectValueInUse(multiValue: InitialFormState) {
  const { use } = multiValue;

  return multiValue[VALUE_TO_USE[use]];
}

function initialValue(
  initialFormState: InitialFormState,
  {
    field,
    name,
    nameUse,
    isEnabled,
  }: {
    field: Field<string>;
    name: string;
    nameUse: string;
    isEnabled: boolean;
  },
) {
  const { use } = initialFormState;

  const selectedValue = selectValueInUse(initialFormState);
  const hereValue = selectValueInUse({
    ...initialFormState,
    use: HERE_SETTINGS,
  });

  // Handle cases when client setting has the value set as NaN
  if (isNaN(selectedValue)) {
    return {
      enabled: isEnabled ?? true, // Show the fields to allow changing
      [name]: selectedValue,
      [nameUse]: use,
      here: hereValue,
    };
  }

  return {
    enabled: isEnabled ?? selectedValue >= 0,
    [name]: field.transformer?.ui(selectedValue) ?? selectedValue,
    here: field.transformer?.ui(hereValue) ?? selectedValue,
    [nameUse]: use,
  };
}

export function CheckboxFieldDropdown({
  name,
  checkboxLabel,
  dropdownLabel,
  field,
  validationMessage,
  initialFormState,
}: {
  name: string;
  checkboxLabel?: string;
  dropdownLabel: string;
  field: Field<string>;
  initialFormState: InitialFormState;
  validationMessage?: string;
}) {
  const nameUse = `${name}.use`;

  const [formData, setFormData] = useState(() =>
    initialValue(initialFormState, {
      field,
      name,
      nameUse,
      // If no checkbox, keep the fields enabled by default
      isEnabled: checkboxLabel ? undefined : true,
    }),
  );

  const checky = {
    checkbox: {
      name: `enable_${name}`,
      label: checkboxLabel,
      value: formData.enabled,
    },
    field: {
      ...field,
      type: "number",
      value: formData[name],
    },
    dropdown: {
      name: nameUse,
      label: dropdownLabel,
      value: formData[nameUse],
    },
  };

  const getHereValue = () => {
    // Send no changes to "HERE" value if the selected source is not HERE.
    // Even if the field was interacted with.
    if (+formData[nameUse] !== HERE_SETTINGS) {
      const hereValue = selectValueInUse({
        ...initialFormState,
        use: HERE_SETTINGS,
      });

      return hereValue;
    }

    return field.transformer?.api(formData["here"]) ?? formData["here"];
  };

  const hiddenInputValues = {
    value: getHereValue(),
    use: formData[nameUse],
  };

  return (
    <div
      style={{
        "--_margin": 0,
      }}
    >
      {checkboxLabel && (
        <CheckboxFieldUncontrolled
          style={{
            marginInlineStart: "-6px",
          }}
          key={checky.checkbox.name}
          id={checky.checkbox.name}
          label={checky.checkbox.label}
          checked={formData.enabled}
          onChange={(_, d) => {
            setFormData((p) => ({
              ...p,
              enabled: d.checked,
              [name]: d.checked ? Math.abs(p[name]) : Math.abs(p[name]) * -1,
              here: d.checked ? Math.abs(p[name]) : Math.abs(p[name]) * -1,
              [nameUse]: HERE_SETTINGS,
            }));
          }}
        />
      )}
      <input type="hidden" name={name} value={hiddenInputValues.value} />
      <input type="hidden" name={nameUse} value={hiddenInputValues.use} />
      {formData.enabled && (
        <div className="">
          <TextFieldDropdown
            key={checky.field.name}
            label={checky.field.label}
            description={checky.field.description}
            validationMessage={validationMessage}
            type="text"
            inputProps={{
              ...checky.field.inputProps,
              value: formData[name],
              onChange: (_, { value }) => {
                setFormData((p) => ({
                  ...p,
                  [name]: value,
                  here: value,
                  // Switch used settings to HERE if input is changed
                  [nameUse]: HERE_SETTINGS,
                }));
              },
            }}
          >
            <DropdownField
              id={nameUse}
              label={dropdownLabel}
              value={
                FORMATTED_VALUE_TO_USE[VALUE_TO_USE[formData[nameUse]]].name
              }
              selectedOptions={[String(formData[nameUse])]}
              onOptionSelect={(e, d) => {
                const newUse = d.optionValue;

                const newValue =
                  selectValueInUse({
                    ...initialFormState,
                    use: newUse,
                  }) ?? "";

                const transformedValue =
                  field.transformer?.ui(+newValue) ?? newValue;

                setFormData((p) => ({
                  ...p,
                  [name]: transformedValue,
                  [nameUse]: newUse,
                }));
              }}
            />
          </TextFieldDropdown>
        </div>
      )}
    </div>
  );
}

function DropdownField({
  label,
  ...props
}: {
  label: string;
} & Partial<DropdownProps>) {
  return (
    <div>
      <Label htmlFor={props.id} className="visually-hidden">
        {label}
      </Label>
      <Dropdown
        // Set explicitly to prevent validation of parent Field affecting this field
        aria-invalid={false}
        onOptionSelect={(e, d) => {
          props?.onOptionSelect?.(e, d);
        }}
        positioning="below-end"
        inlinePopup
        style={{
          minWidth: 0,
          width: "10ch",
        }}
        listbox={{
          style: {
            minWidth: "max-content",
          },
        }}
        {...props}
      >
        {Object.entries(VALUE_TO_USE).map(([k, v]) => (
          <Option key={k} value={k} text={FORMATTED_VALUE_TO_USE[v].name}>
            <div>
              <Body1 block>{FORMATTED_VALUE_TO_USE[v].name}</Body1>
              <Body1
                as="p"
                style={{
                  color: "var(--colorNeutralForeground3)",
                }}
              >
                {FORMATTED_VALUE_TO_USE[v].desc}
              </Body1>
            </div>
          </Option>
        ))}
      </Dropdown>
    </div>
  );
}

function TextFieldDropdown({
  label,
  hint,
  name,
  type,
  validationMessage,
  inputProps,
  children,
}: {
  label: string;
  hint?: string;
  name: string;
  type: InputProps["type"];
  validationMessage?: string;
  inputProps?: InputProps;
  children?: React.ReactNode;
}) {
  const initialValue = useRef(inputProps?.value);

  const [_value, setValue] = useState(initialValue.current);

  const showHint = !validationMessage;

  const inputValue = inputProps?.value ?? _value;

  const hasValueChanged = _value != initialValue.current;

  const resetToInitialValue = () => {
    {
      const newValue = initialValue.current;

      setValue(newValue);
      inputProps?.onChange?.(null, { value: newValue });
    }
  };

  return (
    <FUIField
      hint={showHint ? hint : undefined}
      label={label}
      {...(validationMessage && {
        validationState: "error",
        validationMessage,
      })}
      orientation="horizontal"
      className={styles.field}
    >
      <div className={styles["reset-input"]}>
        <Button
          appearance="subtle"
          icon={<ArrowResetRegular />}
          data-hidden={!hasValueChanged}
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
        {children}
      </div>
    </FUIField>
  );
}

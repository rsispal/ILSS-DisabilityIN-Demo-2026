import type { ComponentType, ReactNode, CSSProperties, InputHTMLAttributes, ChangeEvent } from 'react';

export interface ForgeCommonNamespace {
  Header: ComponentType<{
    logo?: ReactNode;
    rightSlot?: ReactNode;
  }>;
  Logo: ComponentType<{
    icon?: boolean;
    iconVariant?: string;
    style?: CSSProperties;
  }>;
  Avatar: ComponentType<{
    size?: string;
    text?: string;
    alt?: string;
    interactive?: boolean;
  }>;
  Modal: ComponentType<{
    backdrop?: boolean;
    label?: string;
    title?: string;
    onModalClose?: () => void;
    onBackdropClick?: () => void;
    footerRight?: ReactNode;
    children?: ReactNode;
  }>;
  ModalContent: ComponentType<{
    padded?: boolean;
    style?: CSSProperties;
    children?: ReactNode;
  }>;
  Button: ComponentType<{
    variant?: string;
    size?: string;
    onClick?: () => void;
    disabled?: boolean;
    style?: CSSProperties;
    children?: ReactNode;
  }>;
  Control: ComponentType<{ children?: ReactNode }>;
  Label: ComponentType<{ htmlFor?: string; children?: ReactNode }>;
  Input: ComponentType<InputHTMLAttributes<HTMLInputElement>>;
  Toggle: ComponentType<{
    id?: string;
    checked?: boolean;
    onChange?: (ev: ChangeEvent<HTMLInputElement>) => void;
  }>;
}

declare global {
  interface Window {
    ForgeCommon: ForgeCommonNamespace;
  }
}

export {};

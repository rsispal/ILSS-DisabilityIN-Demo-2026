import { createElement, type ComponentProps } from 'react';
import type { ForgeCommonNamespace } from '@/types/forge';

function getForge(): ForgeCommonNamespace {
  if (!window.ForgeCommon) {
    throw new Error('ForgeCommon is not loaded. Ensure bootstrap.ts runs before importing DS components.');
  }
  return window.ForgeCommon;
}

export function Header(props: ComponentProps<ForgeCommonNamespace['Header']>) {
  return createElement(getForge().Header, props);
}

export function Logo(props: ComponentProps<ForgeCommonNamespace['Logo']>) {
  return createElement(getForge().Logo, props);
}

export function Avatar(props: ComponentProps<ForgeCommonNamespace['Avatar']>) {
  return createElement(getForge().Avatar, props);
}

export function Modal(props: ComponentProps<ForgeCommonNamespace['Modal']>) {
  return createElement(getForge().Modal, props);
}

export function ModalContent(props: ComponentProps<ForgeCommonNamespace['ModalContent']>) {
  return createElement(getForge().ModalContent, props);
}

export function Button(props: ComponentProps<ForgeCommonNamespace['Button']>) {
  return createElement(getForge().Button, props);
}

export function Control(props: ComponentProps<ForgeCommonNamespace['Control']>) {
  return createElement(getForge().Control, props);
}

export function Label(props: ComponentProps<ForgeCommonNamespace['Label']>) {
  return createElement(getForge().Label, props);
}

export function Input(props: ComponentProps<ForgeCommonNamespace['Input']>) {
  return createElement(getForge().Input, props);
}

export function Toggle(props: ComponentProps<ForgeCommonNamespace['Toggle']>) {
  return createElement(getForge().Toggle, props);
}

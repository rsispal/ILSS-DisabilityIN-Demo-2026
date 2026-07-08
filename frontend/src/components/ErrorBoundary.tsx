import { Component, type ErrorInfo, type ReactNode } from 'react';

interface ErrorBoundaryProps {
  children: ReactNode;
}

interface ErrorBoundaryState {
  hasError: boolean;
  error: Error | null;
}

export class ErrorBoundary extends Component<ErrorBoundaryProps, ErrorBoundaryState> {
  state: ErrorBoundaryState = { hasError: false, error: null };

  static getDerivedStateFromError(error: Error): ErrorBoundaryState {
    return { hasError: true, error };
  }

  componentDidCatch(error: Error, info: ErrorInfo) {
    console.error('ILSS simulator error:', error, info.componentStack);
  }

  private handleRetry = () => {
    this.setState({ hasError: false, error: null });
  };

  render() {
    if (!this.state.hasError) return this.props.children;

    return (
      <div className="error-boundary">
        <div className="error-boundary-card">
          <h1>Something went wrong</h1>
          <p>The simulator hit an unexpected error. You can reload the page or try again.</p>
          {this.state.error && (
            <pre className="error-boundary-detail">{this.state.error.message}</pre>
          )}
          <div className="error-boundary-actions">
            <button type="button" onClick={this.handleRetry}>
              Try again
            </button>
            <button type="button" onClick={() => window.location.reload()}>
              Reload page
            </button>
          </div>
        </div>
      </div>
    );
  }
}

import { BrowserRouter, Routes, Route } from 'react-router-dom';
import { AppProvider } from './context/AppContext';
import { BleTwinProvider } from './context/BleTwinContext';
import { LanyardSimulatorPage } from './pages/LanyardSimulatorPage';

export default function App() {
  return (
    <AppProvider>
      <BleTwinProvider>
        <BrowserRouter>
          <Routes>
            <Route path="/" element={<LanyardSimulatorPage />} />
          </Routes>
        </BrowserRouter>
      </BleTwinProvider>
    </AppProvider>
  );
}
